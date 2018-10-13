using System;

namespace BMRawYuv420p10ToDng {
    class Program {
        static void Main(string[] args) {
            if (args.Length != 2) {
                Console.WriteLine("Usage: BMRawYuv420p10ToDng fromYuv420p10ImageFilePath toDngFilePath");
                return;
            }

            var conv = new Convert();
            conv.Run(args[0], args[1]);
        }
    }
}
