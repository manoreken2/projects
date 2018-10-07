using System;

namespace DngHeaderDump {
    class Program {
        static void Main(string[] args) {
            if (args.Length != 1) {
                Console.WriteLine("Usage: DngHeaderDump fromDngFile");
                return;
            }

            var r = new DngRW.DngReader();
            r.ReadHeader(args[0]);
        }
    }
}
