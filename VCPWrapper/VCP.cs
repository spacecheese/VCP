﻿/*
 * Author: James Watson 20/01/19
 * Licence: MIT (See License.txt or https://opensource.org/licenses/MIT)
 * 
 * Note: Targeted for .NET Framework 4.5 as lab PCs do not support .NET Standard 2.0 (required for System.Timers) at time of writing. This may change in future.
 */
using System;
using System.Text;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Timers;

namespace VCPWrapper
{
    // See: https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/enumeration-types.
    public enum ParityBit : byte
    {
        NoParity = 0,
        OddParity = 1,
        EvenParity = 2
    }

    public enum StopBits : byte
    {
        One = 1,
        Two = 2
    }

    public class DataRecivedEventArgs : EventArgs
    {
        public readonly byte[] Data;

        public DataRecivedEventArgs(byte[] data) : base()
        {
            Data = data;
        }

        public string DataToString()
        {
            return Encoding.ASCII.GetString(Data);
        }
    }

    public class Port : IDisposable
    {
        private static class NativeMethods
        {
            // For CharSet attribute see https://stackoverflow.com/a/289115.
            // For CallingConvention attribute see https://stackoverflow.com/a/9693642.
            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern int init_COM_port(byte[] portName, int baudRate, byte parityBit, byte stopBits, byte mode);

            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern int poll_COM_port(byte[] buffer, short length);

            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern int COM_port_send_buffer(byte[] buffer, byte length);

            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern int COM_port_send_byte(byte data);

            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern int COM_port_send_word(ushort data);

            [DllImport("VCP.dll", SetLastError = true, CallingConvention = CallingConvention.Cdecl)]
            public static extern void close_COM_port();
        }

        /// <summary>
        /// BaudRates supported by VCP.
        /// </summary>
        // Copied from VCP.c
        public static readonly int[] BaudRates = { 110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 128000, 256000 };

        public string PortName { get; private set; }
        public int BaudRate { get; private set; }
        public ParityBit Parity { get; private set; }
        public StopBits StopBits { get; private set; }
        public bool PortOpen { get; private set; } = false;

        private readonly Timer dataPoller;
        private const double pollRate = 500;
        private const short bufferLength = 500;


        /// <summary>
        /// Fired as new data is read from the Port input buffer.
        /// </summary>
        public event EventHandler<DataRecivedEventArgs> DataRecived;

        /// <summary>
        /// Creates (but doesn't open) a new port object. Use <see cref="Open(string, int, ParityBit, StopBits)"/> to open the specified port.
        /// </summary>
        /// <param name="portName">The name of the port to open (eg COM1).</param>
        /// <param name="baudRate">The desired baud rate. For supported rates see <see cref="BaudRates"/>.</param>
        /// <param name="parityBit">The parity bit configuration.</param>
        /// <param name="stopBits">The number of stop bits to append to every message.</param>
        public Port(string portName, int baudRate = 9600, ParityBit parityBit = ParityBit.NoParity, StopBits stopBits = StopBits.One)
        {
            if (!(Array.BinarySearch(BaudRates, baudRate) > 0))
                throw new ArgumentException("Unsupported baudRate.");

            PortName = portName;
            BaudRate = baudRate;
            Parity = parityBit;
            StopBits = stopBits;

            dataPoller = new Timer(pollRate);
            dataPoller.Elapsed += Poll;
        }

        // Autogenerated signature.
        private void Poll(object sender, ElapsedEventArgs e)
        {
            byte[] buffer = new byte[bufferLength];
            int bytesRecived = NativeMethods.poll_COM_port(buffer, bufferLength);

            if (bytesRecived < 0)
                // Negative length- error has occurred.
                throw new Win32Exception(Marshal.GetLastWin32Error());
            else if (bytesRecived > 0)
            {
                if(DataRecived != null)
                {
                    // Crop buffer to only include recived characters.
                    byte[] croppedBuffer = new byte[bytesRecived];
                    Array.Copy(buffer, croppedBuffer, bytesRecived);

                    DataRecived?.Invoke(this, new DataRecivedEventArgs(croppedBuffer));
                }

                if (bytesRecived == bufferLength)
                    // Data may exceed buffer size. Poll until all data has been read.
                    Poll(this, e);
            }
        }

        /// <summary>
        /// Opens this port using VCP.
        /// </summary>
        public void Open()
        {
            if (PortOpen)
                return;
            if (PortName == null)
                throw new ArgumentNullException("portName");

            if (NativeMethods.init_COM_port(Encoding.ASCII.GetBytes(PortName), BaudRate, (byte)Parity, (byte)StopBits, 0) != 0)
                throw new Win32Exception(Marshal.GetLastWin32Error());

            PortOpen = true;

            dataPoller.Start();
        }

        /// <summary>
        /// Closes this port.
        /// </summary>
        public void Close()
        {
            if (!PortOpen) return;

            NativeMethods.close_COM_port();

            PortOpen = false;
        }

        /// <summary>
        /// Sends a specified array of bytes.
        /// </summary>
        public void SendBytes(byte[] data)
        {
            if (!PortOpen) throw new InvalidOperationException("Cannot write data to an unopened port.");

            if (data == null) throw new ArgumentNullException("data");

            if (data.Length == 0) throw new ArgumentException("data must contain 1 or more bytes.");

            if (data.Length > byte.MaxValue) throw new ArgumentException(String.Format("data must be less than {0} bytes in length.", byte.MaxValue));

            if(NativeMethods.COM_port_send_buffer(data, (byte)data.Length) < 0)
                throw new Win32Exception(Marshal.GetLastWin32Error());
        }

        /// <summary>
        /// Writes a word (two bytes) to the port. Use <see cref="SendBytes(byte[])"/> to send more than two.
        /// </summary>
        public void SendBytes(ushort data)
        {
            if (!PortOpen) throw new InvalidOperationException("Cannot write data to an unopened port.");

            if (NativeMethods.COM_port_send_word(data) < 0)
                throw new Win32Exception(Marshal.GetLastWin32Error());
        }

        /// <summary>
        /// Writes a single byte to the COM port. Use <see cref="SendBytes(byte[])"/> or <see cref="SendBytes(ushort)"/> to send more than one.
        /// </summary>
        public void SendByte(byte data)
        {
            if (!PortOpen) throw new InvalidOperationException("Cannot write data to an unopened port.");

            if (NativeMethods.COM_port_send_byte(data) < 0)
                throw new Win32Exception(Marshal.GetLastWin32Error());
        }

        /// <summary>
        /// Writes a string to the COM Port. Characters that cannot be encoded in ASCII may not behave as expected.
        /// </summary>
        public void SendString(string data)
        {
            SendBytes(Encoding.ASCII.GetBytes(data));
        }


        #region IDisposable Support
        // Visual Studio IDisposable Pattern.
        // See: https://docs.microsoft.com/en-us/dotnet/standard/garbage-collection/implementing-dispose

        private bool disposedValue = false;

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    dataPoller.Dispose();
                }

                Close();
                disposedValue = true;
            }
        }

        ~Port() {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
        #endregion
    }
}
