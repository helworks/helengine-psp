namespace helengine.psp.builder;

/// <summary>
/// Copies staged cooked artifacts and the produced PBP into the final PSP homebrew app layout.
/// </summary>
public sealed class PspAppLayoutWriter {
    /// <summary>
    /// Writes the PSP homebrew app layout for one prepared workspace.
    /// </summary>
    /// <param name="workspace">Prepared workspace describing the staged assets and native build output.</param>
    public void Write(PspBuildWorkspace workspace) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        } else if (!Directory.Exists(workspace.StagingRootPath)) {
            throw new DirectoryNotFoundException($"Staging root '{workspace.StagingRootPath}' was not found.");
        } else if (!File.Exists(workspace.NativePbpPath)) {
            throw new FileNotFoundException("Native PSP build did not produce an EBOOT.PBP.", workspace.NativePbpPath);
        }

        Directory.CreateDirectory(workspace.AppRootPath);
        Directory.CreateDirectory(workspace.CookedOutputRootPath);

        string[] stagedFilePaths = Directory.GetFiles(workspace.StagingRootPath, "*", SearchOption.AllDirectories);
        Array.Sort(stagedFilePaths, StringComparer.OrdinalIgnoreCase);
        for (int index = 0; index < stagedFilePaths.Length; index++) {
            string stagedFilePath = stagedFilePaths[index];
            string relativePath = Path.GetRelativePath(workspace.StagingRootPath, stagedFilePath);
            string destinationPath = Path.Combine(workspace.AppRootPath, relativePath);
            string destinationDirectoryPath = Path.GetDirectoryName(destinationPath) ?? throw new InvalidOperationException($"Destination directory could not be resolved for '{destinationPath}'.");
            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(stagedFilePath, destinationPath, true);
        }

        File.Copy(workspace.NativePbpPath, workspace.AppEbootPath, true);
    }
}
