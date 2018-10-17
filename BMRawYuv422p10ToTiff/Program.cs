using System;

namespace BMRawYuv422p10ToTiff {
    class Program {
        static void Main(string[] args) {
            if (args.Length != 2) {
                Console.WriteLine("Usage: BMRawYuv422p10ToTiff fromYuv420p10ImageFilePath toTiffFilePathTemplate");
                return;
            }

            var conv = new Convert();
            conv.Run(args[0], args[1]);
        }
    }
}
