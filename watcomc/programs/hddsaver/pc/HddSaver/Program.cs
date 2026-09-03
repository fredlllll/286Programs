using HddSaver.Data;
using Microsoft.EntityFrameworkCore;

namespace HddSaver;

static class Program
{
    /// <summary>
    ///  The main entry point for the application.
    /// </summary>
    [STAThread]
    static void Main()
    {
        // To customize application configuration such as set high DPI settings or default font,
        // see https://aka.ms/applicationconfiguration.
        ApplicationConfiguration.Initialize();

        using var ctx = new HddSaverContext();
        ctx.Database.Migrate();

        Application.Run(new MainForm());
    }    
}