# Writes every platform's icon and background images from icon.png and bg.png
# at the repository root (Windows PowerShell 5.1, no other tool needed):
#   switch/icon.jpg                       NRO icon, 256x256 JPEG
#   ps5/sce_sys/icon0.png                 home-screen tile, 512x512 PNG
#   ps5/sce_sys/pic0.dds, pic1.dds        selection / launch background, 3840x2160 BC7 DDS
#   xbox/package/Assets/*.png             UWP logos and splash screen
#   mac/OutRun.icns                       application icon
#
#   powershell -File tools/make_icons.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class Or2Icons {
    // The source scaled to cover width x height (centre crop), high-quality resampling.
    public static Bitmap Cover(Bitmap source, int width, int height) {
        var result = new Bitmap(width, height, PixelFormat.Format32bppArgb);
        double scale = Math.Max((double)width / source.Width, (double)height / source.Height);
        double w = width / scale, h = height / scale;
        var from = new RectangleF((float)((source.Width - w) / 2), (float)((source.Height - h) / 2), (float)w, (float)h);
        using (var g = Graphics.FromImage(result)) {
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;
            using (var attributes = new ImageAttributes()) {
                attributes.SetWrapMode(WrapMode.TileFlipXY);
                g.DrawImage(source, new Rectangle(0, 0, width, height), from.X, from.Y, from.Width, from.Height, GraphicsUnit.Pixel, attributes);
            }
        }
        return result;
    }

    public static void SavePng(Bitmap image, string path) { image.Save(path, ImageFormat.Png); }

    public static void SaveJpeg(Bitmap image, string path, long quality) {
        ImageCodecInfo codec = null;
        foreach (var c in ImageCodecInfo.GetImageEncoders()) if (c.MimeType == "image/jpeg") codec = c;
        using (var parameters = new EncoderParameters(1)) {
            parameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, quality);
            using (var opaque = new Bitmap(image.Width, image.Height, PixelFormat.Format24bppRgb)) {
                using (var g = Graphics.FromImage(opaque)) g.DrawImage(image, 0, 0, image.Width, image.Height);
                opaque.Save(path, codec, parameters);
            }
        }
    }

    public static byte[] PngBytes(Bitmap image) {
        using (var stream = new MemoryStream()) { image.Save(stream, ImageFormat.Png); return stream.ToArray(); }
    }

    static void Be32(Stream s, int v) { s.WriteByte((byte)(v >> 24)); s.WriteByte((byte)(v >> 16)); s.WriteByte((byte)(v >> 8)); s.WriteByte((byte)v); }

    // Apple icon file: PNG entries ic07 (128) .. ic10 (1024).
    public static void SaveIcns(Bitmap source, string path) {
        var entries = new string[] { "ic07", "ic08", "ic09", "ic10" };
        var sizes = new int[] { 128, 256, 512, 1024 };
        using (var body = new MemoryStream()) {
            for (int i = 0; i < entries.Length; ++i) {
                byte[] png;
                using (var scaled = Cover(source, sizes[i], sizes[i])) png = PngBytes(scaled);
                var type = System.Text.Encoding.ASCII.GetBytes(entries[i]);
                body.Write(type, 0, 4); Be32(body, png.Length + 8); body.Write(png, 0, png.Length);
            }
            using (var file = File.Create(path)) {
                var magic = System.Text.Encoding.ASCII.GetBytes("icns");
                file.Write(magic, 0, 4); Be32(file, (int)body.Length + 8); body.WriteTo(file);
            }
        }
    }

    static readonly int[] Weights = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

    // 7-bit value and p-bit for an 8-bit endpoint colour (one p-bit for all four channels).
    static void Quantize(int[] colour, int[] q, out int pbit) {
        int best = int.MaxValue; pbit = 0;
        for (int p = 0; p < 2; ++p) {
            int err = 0; var t = new int[4];
            for (int c = 0; c < 4; ++c) {
                int v = (colour[c] - p + 1) >> 1; if (v < 0) v = 0; if (v > 127) v = 127;
                t[c] = v; int d = ((v << 1) | p) - colour[c]; err += d * d;
            }
            if (err < best) { best = err; pbit = p; Array.Copy(t, q, 4); }
        }
    }

    // BC7 mode 6 (one subset, RGBA 7.7.7.7 + p-bit endpoints, 4-bit indices).
    static void EncodeBlock(int[,] px, byte[] output, int offset) {
        int[] lo = { 255, 255, 255, 255 }, hi = { 0, 0, 0, 0 };
        for (int i = 0; i < 16; ++i) for (int c = 0; c < 4; ++c) { lo[c] = Math.Min(lo[c], px[i, c]); hi[c] = Math.Max(hi[c], px[i, c]); }
        int[] q0 = new int[4], q1 = new int[4]; int p0, p1;
        Quantize(lo, q0, out p0); Quantize(hi, q1, out p1);
        int[] e0 = new int[4], e1 = new int[4];
        for (int c = 0; c < 4; ++c) { e0[c] = (q0[c] << 1) | p0; e1[c] = (q1[c] << 1) | p1; }
        var palette = new int[16, 4];
        for (int k = 0; k < 16; ++k) for (int c = 0; c < 4; ++c) palette[k, c] = ((64 - Weights[k]) * e0[c] + Weights[k] * e1[c] + 32) >> 6;
        var index = new int[16];
        for (int i = 0; i < 16; ++i) {
            int best = int.MaxValue;
            for (int k = 0; k < 16; ++k) {
                int err = 0; for (int c = 0; c < 4; ++c) { int d = palette[k, c] - px[i, c]; err += d * d; }
                if (err < best) { best = err; index[i] = k; }
            }
        }
        if (index[0] >= 8) {   // the anchor index has no top bit: swap the endpoints
            var t = q0; q0 = q1; q1 = t; int tp = p0; p0 = p1; p1 = tp;
            for (int i = 0; i < 16; ++i) index[i] = 15 - index[i];
        }
        ulong lowBits = 0, highBits = 0; int position = 0;
        Action<ulong, int> put = (value, bits) => {
            for (int b = 0; b < bits; ++b, ++position) {
                ulong bit = (value >> b) & 1;
                if (position < 64) lowBits |= bit << position; else highBits |= bit << (position - 64);
            }
        };
        put(1ul << 6, 7);
        for (int c = 0; c < 4; ++c) { put((ulong)q0[c], 7); put((ulong)q1[c], 7); }
        put((ulong)p0, 1); put((ulong)p1, 1);
        put((ulong)index[0], 3);
        for (int i = 1; i < 16; ++i) put((ulong)index[i], 4);
        BitConverter.GetBytes(lowBits).CopyTo(output, offset);
        BitConverter.GetBytes(highBits).CopyTo(output, offset + 8);
    }

    // Single-level BC7_UNORM DX10 DDS of the image (dimensions multiple of 4).
    public static void SaveBc7Dds(Bitmap image, string path) {
        int width = image.Width, height = image.Height, bx = width / 4, by = height / 4;
        var data = image.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var bgra = new byte[width * height * 4];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, bgra, 0, bgra.Length);
        image.UnlockBits(data);
        var blocks = new byte[bx * by * 16];
        System.Threading.Tasks.Parallel.For(0, by, y => {
            var px = new int[16, 4];
            for (int x = 0; x < bx; ++x) {
                for (int j = 0; j < 4; ++j) for (int i = 0; i < 4; ++i) {
                    int o = ((y * 4 + j) * width + x * 4 + i) * 4;
                    px[j * 4 + i, 0] = bgra[o + 2]; px[j * 4 + i, 1] = bgra[o + 1]; px[j * 4 + i, 2] = bgra[o]; px[j * 4 + i, 3] = bgra[o + 3];
                }
                EncodeBlock(px, blocks, (y * bx + x) * 16);
            }
        });
        var header = new byte[148];
        Action<int, int> u32 = (offset, value) => BitConverter.GetBytes(value).CopyTo(header, offset);
        System.Text.Encoding.ASCII.GetBytes("DDS ").CopyTo(header, 0);
        u32(4, 124); u32(8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000); u32(12, height); u32(16, width);
        u32(20, blocks.Length); u32(28, 1);
        u32(76, 32); u32(80, 0x4); System.Text.Encoding.ASCII.GetBytes("DX10").CopyTo(header, 84);
        u32(108, 0x1000);
        u32(128, 98); u32(132, 3); u32(140, 1);
        using (var file = File.Create(path)) { file.Write(header, 0, header.Length); file.Write(blocks, 0, blocks.Length); }
    }
}
'@

$icon = [System.Drawing.Bitmap]::FromFile((Join-Path $root 'icon.png'))
$bg = [System.Drawing.Bitmap]::FromFile((Join-Path $root 'bg.png'))
function Out-Image($image, $path, $kind) {
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    switch ($kind) {
        'png' { [Or2Icons]::SavePng($image, $path) }
        'jpg' { [Or2Icons]::SaveJpeg($image, $path, 92) }
        'dds' { [Or2Icons]::SaveBc7Dds($image, $path) }
    }
    $image.Dispose()
    Write-Host "  $($path.Substring($root.Length + 1))"
}

Out-Image ([Or2Icons]::Cover($icon, 256, 256)) (Join-Path $root 'switch\icon.jpg') 'jpg'
Out-Image ([Or2Icons]::Cover($icon, 512, 512)) (Join-Path $root 'ps5\sce_sys\icon0.png') 'png'
Out-Image ([Or2Icons]::Cover($bg, 3840, 2160)) (Join-Path $root 'ps5\sce_sys\pic0.dds') 'dds'
Copy-Item (Join-Path $root 'ps5\sce_sys\pic0.dds') (Join-Path $root 'ps5\sce_sys\pic1.dds') -Force
Write-Host '  ps5\sce_sys\pic1.dds'
$assets = Join-Path $root 'xbox\package\Assets'
Out-Image ([Or2Icons]::Cover($icon, 150, 150)) (Join-Path $assets 'Square150x150Logo.png') 'png'
Out-Image ([Or2Icons]::Cover($icon, 44, 44)) (Join-Path $assets 'Square44x44Logo.png') 'png'
Out-Image ([Or2Icons]::Cover($icon, 50, 50)) (Join-Path $assets 'StoreLogo.png') 'png'
Out-Image ([Or2Icons]::Cover($bg, 310, 150)) (Join-Path $assets 'Wide310x150Logo.png') 'png'
Out-Image ([Or2Icons]::Cover($bg, 620, 300)) (Join-Path $assets 'SplashScreen.png') 'png'
[Or2Icons]::SaveIcns($icon, (Join-Path $root 'mac\OutRun.icns'))
Write-Host '  mac\OutRun.icns'
$icon.Dispose(); $bg.Dispose()
