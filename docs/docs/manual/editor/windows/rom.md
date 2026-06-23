# ROM (Size Dashboard)

The **ROM** window breaks down how much cartridge space your built game uses, by asset
category and by individual file. It is useful for keeping a project within a target cartridge
size and for spotting which assets are the largest.

The data comes from the last build. Until you have built the project, the window shows
"No build data": build the game, then press **Refresh**.

## Toolbar

| Control | Description |
|---------|-------------|
| **Cart** | The target cartridge size to budget against. |
| **Refresh** | Re-scan the build output and update the breakdown. |

## Budget bar

A stacked bar shows total usage against the selected cartridge size, with a percentage. Each
segment is one asset category, color-coded. If the project exceeds the cartridge size, a red
line marks the limit.

Below the bar is a per-category summary listing each category and its total size.

## Asset table

The table lists every asset file with its **Type**, **Name**, **Size** and **Compression**.
Click a column header to sort (for example by Size, to find the biggest assets).

The categories are Texture, Model, Audio, Font, Scene, Code and Other.

## See also

- {doc}`Log <log>`: build output and errors.
- {doc}`Assets <../../assets>`: asset types and how they are imported.
