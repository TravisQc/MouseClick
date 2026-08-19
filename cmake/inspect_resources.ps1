param(
    [Parameter(Mandatory = $true)]
    [string]$Executable
)

$source = @'
using System;
using System.Runtime.InteropServices;

public static class NativeResources
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryEx(string fileName, IntPtr file, uint flags);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);

    [DllImport("kernel32.dll")]
    public static extern bool FreeLibrary(IntPtr module);
}
'@

Add-Type -TypeDefinition $source
$loadLibraryAsDataFile = 0x00000002
$module = [NativeResources]::LoadLibraryEx($Executable, [IntPtr]::Zero, $loadLibraryAsDataFile)
if ($module -eq [IntPtr]::Zero) {
    Write-Error "Unable to load PE resources: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    exit 1
}

try {
    $icon = [NativeResources]::FindResource($module, [IntPtr]101, [IntPtr]14)
    $version = [NativeResources]::FindResource($module, [IntPtr]1, [IntPtr]16)
    $manifest = [NativeResources]::FindResource($module, [IntPtr]1, [IntPtr]24)
    if ($icon -eq [IntPtr]::Zero -or $version -eq [IntPtr]::Zero -or
        $manifest -eq [IntPtr]::Zero) {
        Write-Error "Required icon, version, or manifest resource is missing"
        exit 1
    }
    Write-Output "icon=present version=present manifest=present"
}
finally {
    [void][NativeResources]::FreeLibrary($module)
}
