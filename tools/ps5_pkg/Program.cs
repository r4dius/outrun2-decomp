// OutRunPs5Pkg: packs the PS5 title folder (tools/build_ps5_native.sh) into an
// installable package. The package holds only the port; the player sends the
// game folder by FTP and the title prepares OR2006C2C.cache at first launch.
//
//   OutRunPs5Pkg <title folder (PPSA99106)> <output .pkg>
using System.Text.Json;
using LibProsperoPkg;

if (args.Length != 2) { Console.Error.WriteLine("usage: OutRunPs5Pkg <title folder> <output .pkg>"); return 2; }
string app = Path.GetFullPath(args[0]), pkg = Path.GetFullPath(args[1]);
string output = Path.GetDirectoryName(pkg)!;
Directory.CreateDirectory(output);
using var param = JsonDocument.Parse(File.ReadAllText(Path.Combine(app, "sce_sys", "param.json")));
var root = param.RootElement;
var result = ProsperoPackageBuilder.Build(new ProsperoBuildOptions
{
    Mode = ProsperoPackageMode.Application,
    OutputFormat = ProsperoOutputFormat.DebugImage,
    SourceFolder = app,
    OutputFolder = output,
    ContentId = root.GetProperty("contentId").GetString()!,
    TitleId = root.GetProperty("titleId").GetString()!,
    Title = root.GetProperty("localizedParameters").GetProperty("en-US").GetProperty("titleName").GetString()!,
    Version = "01.00",
    // Homebrew package for the owner's console: no license file needed.
    LicenseFree = true,
}, _ => { });
if (!string.Equals(result.OutputPath, pkg, StringComparison.OrdinalIgnoreCase))
{
    if (File.Exists(pkg)) File.Delete(pkg);
    File.Move(result.OutputPath, pkg);
}
Console.WriteLine($"PS5 package: {pkg}");
return 0;
