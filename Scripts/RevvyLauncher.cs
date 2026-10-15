using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows.Forms;

static class Program
{
    [STAThread]
    static void Main()
    {
        string root = AppDomain.CurrentDomain.BaseDirectory;
        string appDir = Path.Combine(root, @"Plugins\Revvy\Binaries\RevvyApp");

        if (Directory.Exists(appDir))
        {
            // Pick newest built app exe so version bumps keep working (proxy exe has a different prefix)
            string exe = Directory.GetFiles(appDir, "Revvy-*.exe")
                                  .OrderByDescending(f => File.GetLastWriteTimeUtc(f))
                                  .FirstOrDefault();
            if (exe != null)
            {
                Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = appDir, UseShellExecute = true });
                return;
            }
        }

        string vbs = Path.Combine(root, @"Plugins\Revvy\RevvyApp.vbs");
        if (File.Exists(vbs))
        {
            Process.Start(new ProcessStartInfo("wscript.exe", "\"" + vbs + "\"") { UseShellExecute = true });
            return;
        }

        MessageBox.Show(
            "Revvy app not found.\nBuild it first: Plugins\\Revvy\\BuildRevvyApp.ps1",
            "Revvy", MessageBoxButtons.OK, MessageBoxIcon.Error);
    }
}
