/*

Yarrboard Framework HTML Builder

Copyright (C) 2016-2017 by Xose Pérez <xose dot perez at gmail dot com>;
Copyright (C) 2025 by Zach Hoeken <hoeken@gmail.com>;

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

import gulp from 'gulp';
const { series, src, dest } = gulp;
import htmlmin from 'gulp-html-minifier-terser';
import cleancss from 'gulp-clean-css';
// import uglify from 'gulp-uglify-es';
import gzip from 'gulp-gzip';
import { deleteAsync } from 'del';
import inline from 'gulp-inline';
import inlineImages from 'gulp-css-base64';
import favicon from 'gulp-base64-favicon';
import { readFileSync, createWriteStream, readdirSync, existsSync, mkdirSync, statSync, rmSync } from 'fs';
import { createHash } from 'crypto';
import { join, basename, relative, dirname, sep } from 'path';
import { lookup as mimeLookup } from 'mime-types';

// ============================================================================
// Configuration
// ============================================================================

// Get the YarrboardFramework path from environment variable
// This is set by the gulp.py script when run via PlatformIO
const FRAMEWORK_PATH = process.env.YARRBOARD_FRAMEWORK_PATH;

if (!FRAMEWORK_PATH) {
    console.error('ERROR: YARRBOARD_FRAMEWORK_PATH environment variable not set!');
    console.error('This should be set by scripts/gulp.py when running via PlatformIO.');
    process.exit(1);
}

// Get the project path - defaults to current directory if not set
// This is where project-specific assets (html/, src/, etc.) are located
const PROJECT_PATH = process.env.YARRBOARD_PROJECT_PATH || '.';

// Check if minification should be enabled
const ENABLE_MINIFY = process.env.YARRBOARD_ENABLE_MINIFY !== undefined;

console.log(`Using YarrboardFramework from: ${FRAMEWORK_PATH}`);
console.log(`Using project path: ${PROJECT_PATH}`);
console.log(`HTML minification: ${ENABLE_MINIFY ? 'ENABLED' : 'DISABLED'}`);

// Determine the output directory for generated headers
// If src/ folder exists, use src/gulp, otherwise use gulp/ at top level
const srcDir = join(PROJECT_PATH, 'src');
const gulpOutputDir = existsSync(srcDir)
    ? join(srcDir, 'gulp')
    : join(PROJECT_PATH, 'gulp');

const PATHS = {
    frameworkHtml: join(FRAMEWORK_PATH, 'html'),  // Framework HTML/CSS/JS
    projectHtml: join(PROJECT_PATH, 'html'),      // Project-specific assets (logos, CSS, JS)
    tempOutput: join(PROJECT_PATH, 'temp'),       // Temporary output directory
    gulpOutput: gulpOutputDir                     // Generated headers directory
};

// Files to ignore when scanning for assets
const IGNORE_FILES = ['index.html', "site.webmanifest", "ws", "api/endpoint", "api/config", "api/stats", "api/update", "coredump.bin"];

console.log('PATHS configuration:');
console.log(`  frameworkHtml: ${PATHS.frameworkHtml}`);
console.log(`  projectHtml: ${PATHS.projectHtml}`);
console.log(`  tempOutput: ${PATHS.tempOutput}`);
console.log(`  gulpOutput: ${PATHS.gulpOutput}`);

// Ensure a fresh output directory
if (existsSync(PATHS.gulpOutput)) {
    rmSync(PATHS.gulpOutput, { recursive: true, force: true });
}
mkdirSync(PATHS.gulpOutput, { recursive: true });

// Recursively scan directory for files matching extension
function scanDirectory(dir, extension, baseDir = dir) {
    const files = [];

    if (!existsSync(dir)) {
        console.log(`  Directory does not exist: ${dir}`);
        return files;
    }

    const entries = readdirSync(dir);
    // console.log(`  Scanning directory: ${dir} (${entries.length} entries)`);

    for (const entry of entries) {
        const fullPath = join(dir, entry);
        const stat = statSync(fullPath);

        if (stat.isDirectory()) {
            // console.log(`    Found subdirectory: ${entry}`);
            files.push(...scanDirectory(fullPath, extension, baseDir));
        } else if (entry.endsWith(extension)) {
            // Store relative path from base directory for better organization
            const relativePath = relative(baseDir, fullPath);
            // console.log(`    Found matching file: ${entry} -> ${relativePath}`);
            files.push(relativePath);
        } else {
            // console.log(`    Skipping file: ${entry} (doesn't end with ${extension})`);
        }
    }

    return files;
}

// Find all files matching a filter function, checking project directory first, then framework
function findFiles(filterFn, ignoreList = []) {
    const files = [];

    // Helper to recursively scan a directory and add matching files
    const scanDir = (dir, source, baseDir = dir) => {
        if (!existsSync(dir)) return;

        const entries = readdirSync(dir);
        entries.forEach(entry => {
            const fullPath = join(dir, entry);
            const stat = statSync(fullPath);

            if (stat.isDirectory()) {
                // Recurse into subdirectories
                scanDir(fullPath, source, baseDir);
            } else {
                // Process files
                const relativePath = relative(baseDir, fullPath);
                const filename = basename(fullPath);

                // Skip hidden files, ignored files, and files that don't match the filter
                if (!filename.startsWith('.') && !ignoreList.includes(relativePath) && filterFn(filename)) {
                    // Only add if not already found (project takes precedence over framework)
                    if (!files.some(f => f.file === relativePath)) {
                        files.push({ file: relativePath, source });
                    }
                } else if (ignoreList.includes(relativePath)) {
                    // console.log(`Ignoring file ${relativePath}`);
                }
            }
        });
    };

    // Check project html directory first (takes precedence)
    scanDir(PATHS.projectHtml, 'project');

    // Then check framework html directory
    scanDir(PATHS.frameworkHtml, 'framework');

    return files.map(f => f.file);
}

// Find project CSS, JS, and other asset files
function findProjectAssets() {
    console.log(`\n=== Scanning for project assets ===`);
    // console.log(`Project HTML path: ${PATHS.projectHtml}`);
    // console.log(`Path exists: ${existsSync(PATHS.projectHtml)}`);

    const assets = {
        css: [],
        js: [],
        files: []
    };

    if (existsSync(PATHS.projectHtml)) {
        // console.log(`Scanning for .css files...`);
        assets.css = scanDirectory(PATHS.projectHtml, '.css');

        // console.log(`Scanning for .js files...`);
        assets.js = scanDirectory(PATHS.projectHtml, '.js');

    } else {
        console.log(`Project HTML directory does not exist, skipping asset scan`);
    }

    // Find other asset files (non-CSS, non-JS, non-hidden)
    // console.log(`\nScanning for other asset files...`);
    assets.files = findFiles(
        file => !file.endsWith('.css') && !file.endsWith('.js'),
        IGNORE_FILES
    );

    if (assets.css.length) {
        console.log(`Found ${assets.css.length} project CSS file(s):`);
        assets.css.forEach(file => console.log(`  - ${file}`));
    }

    if (assets.js.length) {
        console.log(`Found ${assets.js.length} project JS file(s):`);
        assets.js.forEach(file => console.log(`  - ${file}`));
    }

    if (assets.files.length) {
        console.log(`Found ${assets.files.length} gulpable file(s):`);
        assets.files.forEach(file => console.log(`  - ${file}`));
    }

    console.log(`=== Asset scan complete ===\n`);

    return assets;
}

const PROJECT_ASSETS = findProjectAssets();

const HTML_MIN_OPTIONS = {
    collapseWhitespace: true,
    removeComments: true,
    minifyJS: true,
    minifyCSS: true
};

const INLINE_OPTIONS = {
    base: PATHS.frameworkHtml,
    css: [cleancss, inlineImages],
    ignore: [
        'logo.png',        // ignore the apple-touch-icon line
        'site.webmanifest' // ignore the manifest line
    ]
};

// ============================================================================
// Utility Functions
// ============================================================================

// Inject project CSS and JS into the HTML
function injectProjectAssets(htmlPath, assets) {
    if (assets.css.length === 0 && assets.js.length === 0) {
        console.log('No project assets to inject');
        return;
    }

    let html = readFileSync(htmlPath, 'utf8');

    // Inject CSS files into <head>
    if (assets.css.length > 0) {
        let cssContent = '\n<!-- Project CSS -->\n';

        for (const cssFile of assets.css) {
            const cssPath = join(PATHS.projectHtml, cssFile);
            const css = readFileSync(cssPath, 'utf8');
            cssContent += `<!-- ${cssFile} -->\n<style>\n${css}\n</style>\n`;
        }

        // Insert before </head>
        html = html.replace('</head>', cssContent + '</head>');
        // console.log(`Injected ${assets.css.length} CSS file(s)`);
    }

    // Inject JS files before </body>
    if (assets.js.length > 0) {
        let jsContent = '\n<!-- Project JS -->\n';

        for (const jsFile of assets.js) {
            const jsPath = join(PATHS.projectHtml, jsFile);
            const js = readFileSync(jsPath, 'utf8');
            jsContent += `<!-- ${jsFile} -->\n<script>\n${js}\n</script>\n`;
        }

        // Insert before </body>
        html = html.replace('</body>', jsContent + '</body>');
        // console.log(`Injected ${assets.js.length} JS file(s)`);
    }

    // Write the modified HTML back
    createWriteStream(htmlPath).end(html);
}

async function writeHeaderFile(source, destination, name, originalFilename) {
    return new Promise((resolve, reject) => {
        try {
            const wstream = createWriteStream(destination);
            const data = readFileSync(source);

            const hashSum = createHash('sha256');
            hashSum.update(data);
            const hex = hashSum.digest('hex');

            // Determine MIME type from original filename (without .gz extension)
            const mimeType = mimeLookup(originalFilename) || 'application/octet-stream';

            // Write header guard and includes
            const guardName = `GULPED_${name.toUpperCase()}_H`;
            wstream.write(`#ifndef ${guardName}\n`);
            wstream.write(`#define ${guardName}\n\n`);
            wstream.write(`#include "GulpedFile.h"\n\n`);

            // Write the filename (URL-encoded, preserving directory separators)
            // Normalize path separators to forward slashes for URLs (works on all platforms)
            const encodedFilename = originalFilename.split(sep).map(segment => encodeURIComponent(segment)).join('/');
            wstream.write(`const char _${name}_filename[] = "/${encodedFilename}";\n`);

            // Write the MIME type
            wstream.write(`const char _${name}_mimetype[] = "${mimeType}";\n`);

            // Write the SHA256 hash
            wstream.write(`const char _${name}_sha[] = "${hex}";\n`);

            // Write the data array
            wstream.write(`const uint8_t _${name}_data[] = {`);

            for (let i = 0; i < data.length; i++) {
                if (i % 1000 === 0) wstream.write("\n");
                wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
                if (i < data.length - 1) wstream.write(',');
            }

            wstream.write('\n};\n\n');

            // Write the GulpedFile struct
            wstream.write(`const GulpedFile ${name} = {\n`);
            wstream.write(`    _${name}_data,\n`);
            wstream.write(`    ${data.length},\n`);
            wstream.write(`    _${name}_sha,\n`);
            wstream.write(`    _${name}_filename,\n`);
            wstream.write(`    _${name}_mimetype\n`);
            wstream.write(`};\n\n`);

            wstream.write(`#endif // ${guardName}`);
            wstream.end();

            wstream.on('finish', async () => {
                await deleteAsync([source], { force: true });
                resolve();
            });

            wstream.on('error', reject);
        } catch (err) {
            reject(err);
        }
    });
}

// ============================================================================
// Gulp Tasks
// ============================================================================

async function clean() {
    return deleteAsync([PATHS.tempOutput], { force: true });
}

function buildInlineHtml() {
    const indexPath = join(PATHS.frameworkHtml, 'index.html');
    console.log(`Building inline HTML from: ${indexPath}`);

    return src(indexPath)
        .pipe(favicon({ src: PATHS.frameworkHtml }))
        .pipe(inline(INLINE_OPTIONS))
        .pipe(dest(PATHS.tempOutput));
}

async function injectAssets() {
    const htmlPath = join(PATHS.tempOutput, 'index.html');

    if (!existsSync(htmlPath)) {
        console.log('No HTML file found to inject assets into');
        return;
    }

    injectProjectAssets(htmlPath, PROJECT_ASSETS);
}

function minifyAndCompress() {
    let stream = src(join(PATHS.tempOutput, 'index.html'));

    // Only apply htmlmin if YARRBOARD_ENABLE_MINIFY is set
    if (ENABLE_MINIFY) {
        stream = stream.pipe(htmlmin(HTML_MIN_OPTIONS));
    }

    return stream
        .pipe(gzip())
        .pipe(dest(PATHS.tempOutput));
}

async function embedHtml() {
    const source = join(PATHS.tempOutput, 'index.html.gz');
    const destination = join(PATHS.gulpOutput, 'index.html.h');
    await writeHeaderFile(source, destination, 'index_html', 'index.html');
}

function compressFile(filename) {
    // Check project directory first, then framework directory
    let sourcePath;
    if (existsSync(join(PATHS.projectHtml, filename))) {
        sourcePath = join(PATHS.projectHtml, filename);
    } else if (existsSync(join(PATHS.frameworkHtml, filename))) {
        sourcePath = join(PATHS.frameworkHtml, filename);
    } else {
        throw new Error(`File not found: ${filename}`);
    }

    // Get the directory part of the filename to preserve structure
    const dir = filename.includes('/') ? filename.substring(0, filename.lastIndexOf('/')) : '';
    const destPath = dir ? join(PATHS.tempOutput, dir) : PATHS.tempOutput;

    return src(sourcePath)
        .pipe(gzip())
        .pipe(dest(destPath));
}

async function embedFile(filename) {
    const source = join(PATHS.tempOutput, `${filename}.gz`);
    const destination = join(PATHS.gulpOutput, `${filename}.h`);
    const safeName = filename.replace(/[^a-z0-9]/gi, '_');

    // Ensure destination directory exists
    const destDir = dirname(destination);
    if (!existsSync(destDir)) {
        mkdirSync(destDir, { recursive: true });
    }

    await writeHeaderFile(source, destination, safeName, filename);
}

async function generateMetaInclude() {
    const metaIncludePath = join(PATHS.gulpOutput, 'gulped.h');
    const wstream = createWriteStream(metaIncludePath);

    // Collect all files and their struct names
    const allFiles = ['index.html', ...PROJECT_ASSETS.files];
    const structNames = allFiles.map(file => file.replace(/[^a-z0-9]/gi, '_'));

    // Write header guard
    wstream.write(`#ifndef GULPED_H\n`);
    wstream.write(`#define GULPED_H\n\n`);
    wstream.write(`// Auto-generated file - includes all gulped assets\n\n`);

    // Include all header files
    wstream.write(`#include "index.html.h"\n`);
    for (const file of PROJECT_ASSETS.files) {
        const headerFile = `${file}.h`;
        wstream.write(`#include "${headerFile}"\n`);
    }

    wstream.write(`\n`);

    // Create array of pointers to GulpedFile structs
    wstream.write(`// Array of pointers to all gulped files\n`);
    wstream.write(`const GulpedFile* gulpedFiles[] = {\n`);
    for (let i = 0; i < structNames.length; i++) {
        wstream.write(`    &${structNames[i]}`);
        if (i < structNames.length - 1) {
            wstream.write(',');
        }
        wstream.write('\n');
    }
    wstream.write(`};\n\n`);

    // Add count constant
    wstream.write(`// Total number of gulped files\n`);
    wstream.write(`const int gulpedFilesCount = ${structNames.length};\n\n`);

    wstream.write(`#endif // GULPED_H`);
    wstream.end();

    return new Promise((resolve, reject) => {
        wstream.on('finish', () => {
            resolve();
        });
        wstream.on('error', reject);
    });
}

function createFileTask(filename) {
    const compress = () => compressFile(filename);
    const embed = () => embedFile(filename);

    return series(compress, embed);
}

// ============================================================================
// Task Composition
// ============================================================================

const fileTasks = PROJECT_ASSETS.files.map(file => createFileTask(file));
const buildAll = series(
    clean,
    buildInlineHtml,
    injectAssets,
    minifyAndCompress,
    embedHtml,
    ...fileTasks,
    generateMetaInclude,
    clean
);

// ============================================================================
// Exports
// ============================================================================

export {
    clean,
    buildInlineHtml,
    injectAssets,
    minifyAndCompress,
    embedHtml,
    generateMetaInclude,
    buildAll as default
};
