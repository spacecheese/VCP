/*
 * Author: James Watson 20/01/19
 * Licence: MIT (See License.txt or https://opensource.org/licenses/MIT)
 * Tested on: Windows 10 (Build 17763)
 * 
 */
using System;

using VCPWrapper;

namespace Demo
{
    class Program
    {
        static void Main(string[] args)
        {
            // Instance the VCPWrapper.Port
            var port = new Port("COM7");

            // Subscribe to the DataRecived event handler.
            port.DataRecived += DataRecived;

            try
            {
                // Open the Port.
                port.Open();
            }
            catch (Exception ex)
            {
                // Catch any exceptions.
                Console.WriteLine("The port failed to open due to the following error '{0}'", ex.Message);
                Console.WriteLine("Press any key to exit.");
                Console.ReadKey();
                Environment.Exit(-1);
            }

            Console.WriteLine("Port opened sucessfully. Type exit to close.");

            while (true)
            {
                // Read user input from the console.
                string input = Console.ReadLine();
                // Quit if the user requests.
                if (input.Contains("quit") || input.Contains("close") || input.Contains("exit"))
                    break;
                
                // Send the user input through the port.
                // Note: Be careful with character encoding when sending strings. Only ASCII characters are supported in this implementation.
                port.SendString(input + "\n");
            }

            // Close the port when you finish.
            port.Close();
        }

        static void DataRecived(object sender, DataRecivedEventArgs args)
        {
            // Handle the DataRecived event.
            Console.Write(args.DataToString() + "\n");
        }
    }
}
