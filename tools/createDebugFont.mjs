// Node.js script to convert a bitmap font PNG to a C header in IA4 format (4bpp)
// Usage: node createDebugFont.mjs
import fs from 'fs';
import { PNG } from 'pngjs';

const INPUT_PNG = 'data/build/assets/font.ia4.png';
const OUTPUT_HEADER = 'n64/engine/src/debug/font_8x8_IA4.h';
const CHAR_WIDTH = 8;
const CHAR_HEIGHT = 8;
const TEX_WIDTH = 128;
const TEX_HEIGHT = 56;
const CHARS_PER_ROW = 16;
const ROWS = TEX_HEIGHT / CHAR_HEIGHT;
const NUM_CHARS = CHARS_PER_ROW * ROWS;

function rgbaToIA4(r, g, b, a) {
    // Intensity: 3 bits (average of RGB, scaled to 0-7)
    // Alpha: 1 bit (1 if a > 0, else 0)
    const intensity = Math.round(((r + g + b) / 3) / 255 * 7) & 0x7;
    const alpha = a > 0 ? 1 : 0;
    return (intensity << 1) | alpha;
}

function getCharBounds(png, charCol, charRow) {
    let minX = CHAR_WIDTH, maxWhiteX = -1;
    for (let x = 0; x < CHAR_WIDTH; ++x) {
        for (let y = 0; y < CHAR_HEIGHT; ++y) {
            const px = charCol * CHAR_WIDTH + x;
            const py = charRow * CHAR_HEIGHT + y;
            const idx = (py * TEX_WIDTH + px) * 4;
            const r = png.data[idx], g = png.data[idx+1], b = png.data[idx+2], a = png.data[idx+3];
            if (a > 0) {
                if (x < minX) minX = x;
                // Only consider white pixels for maxWhiteX
                if (r > 128) {
                    if (x > maxWhiteX) maxWhiteX = x;
                }
            }
        }
    }
    // If no non-transparent pixel, minX will be CHAR_WIDTH
    // If no white pixel, maxWhiteX will be -1
    if (minX > maxWhiteX || minX === CHAR_WIDTH) return {minX: 0, maxWhiteX: -1, width: 0};
    return {minX, maxWhiteX, width: maxWhiteX - minX + 1};
}

function processFontRowMajorWithWidths(png) {
    // Output: flat array, row-major, scanline-interleaved across all chars, left-aligned, and widths
    const out = [];
    const widths = [];
    for (let charRow = 0; charRow < ROWS; ++charRow) {
        for (let y = 0; y < CHAR_HEIGHT; ++y) {
            for (let charCol = 0; charCol < CHARS_PER_ROW; ++charCol) {
                // Get bounds for this char
                if (y === 0) {
                    const bounds = getCharBounds(png, charCol, charRow);
                    if (widths.length < charRow * CHARS_PER_ROW + charCol + 1) {
                        widths.push(bounds.width+1);
                    }
                }
                const bounds = getCharBounds(png, charCol, charRow);
                // For this scanline, shift left by minX
                for (let x = 0; x < CHAR_WIDTH; x += 2) {
                    // Shifted x
                    let sx1 = x + bounds.minX;
                    let sx2 = x + 1 + bounds.minX;
                    // Clamp to CHAR_WIDTH-1
                    if (sx1 >= CHAR_WIDTH) sx1 = CHAR_WIDTH-1;
                    if (sx2 >= CHAR_WIDTH) sx2 = CHAR_WIDTH-1;
                    const px1x = charCol * CHAR_WIDTH + sx1;
                    const px1y = charRow * CHAR_HEIGHT + y;
                    const idx1 = (px1y * TEX_WIDTH + px1x) * 4;
                    const ia4_1 = rgbaToIA4(
                        png.data[idx1], png.data[idx1+1], png.data[idx1+2], png.data[idx1+3]
                    );
                    const px2x = charCol * CHAR_WIDTH + sx2;
                    const px2y = charRow * CHAR_HEIGHT + y;
                    const idx2 = (px2y * TEX_WIDTH + px2x) * 4;
                    const ia4_2 = rgbaToIA4(
                        png.data[idx2], png.data[idx2+1], png.data[idx2+2], png.data[idx2+3]
                    );
                    out.push((ia4_1 << 4) | ia4_2);
                }
            }
        }
    }
    return {data: out, widths};
}

function writeData(fontData, widths) {
    let out =
`// AUTO-GENERATED FILE | DO NOT MODIFY

namespace FONT8x8
{
    constexpr uint32_t CHAR_WIDTH = ${CHAR_WIDTH};
    constexpr uint32_t CHAR_HEIGHT = ${CHAR_HEIGHT};
    constexpr uint32_t NUM_CHARS = ${NUM_CHARS};
    constexpr uint32_t BYTES_PER_CHAR = ${(CHAR_WIDTH*CHAR_HEIGHT)/2};
    constexpr uint32_t IMG_WIDTH = ${TEX_WIDTH};
    constexpr uint32_t IMG_HEIGHT = ${TEX_HEIGHT};

    // Data is row-major, scanline-interleaved across all chars, left-aligned
    constexpr uint8_t DATA[NUM_CHARS * BYTES_PER_CHAR] = {
`;
    for (let i = 0; i < fontData.length; ++i) {
        out += `0x${fontData[i].toString(16).padStart(2, '0')}` + (i < fontData.length-1 ? ', ' : '');
        if(i % 16 === 15)out += '\n';
    }
    out += '};\n';
    out += 'constexpr uint8_t WIDTHS[NUM_CHARS] = {' + widths.join(",") + '};\n';
    out += '}\n';
    fs.writeFileSync(OUTPUT_HEADER, out);
    console.log(`Wrote ${OUTPUT_HEADER}`);
}

fs.createReadStream(INPUT_PNG)
    .pipe(new PNG())
    .on('parsed', function() {
        if (this.width !== TEX_WIDTH || this.height !== TEX_HEIGHT) {
            throw new Error(`Expected ${TEX_WIDTH}x${TEX_HEIGHT} PNG, got ${this.width}x${this.height}`);
        }
        const {data, widths} = processFontRowMajorWithWidths(this);
        writeData(data, widths);
    });
