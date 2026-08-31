using Microsoft.EntityFrameworkCore;
using HddSaver.Models;

namespace HddSaver.Data;

public class HddSaverContext : DbContext
{
    public DbSet<Sector> Sectors => Set<Sector>();

    protected override void OnConfiguring(DbContextOptionsBuilder options)
    {
        var dbPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "hddsaver.db");
        options.UseSqlite($"Data Source={dbPath}");
    }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<Sector>()
            .Property(s => s.Data)
            .HasColumnType("BLOB");
    }
}
