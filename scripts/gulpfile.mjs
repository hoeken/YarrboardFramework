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
import htmlmin from 'gulp-htmlmin';
import cleancss from 'gulp-clean-css';
// import uglify from 'gulp-uglify-es';
import gzip from 'gulp-gzip';
import { deleteAsync } from 'del';
import inline from 'gulp-inline';
import inlineImages from 'gulp-css-base64';
import favicon from 'gulp-base64-favicon';
import { readFileSync, createWriteStream, readdirSync, existsSync, mkdirSync, statSync } from 'fs';
import { createHash } from 'crypto';
import { join, basename, relative } from 'path';

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

console.log(`Using YarrboardFramework from: ${FRAMEWORK_PATH}`);
console.log(`Using project path: ${PROJECT_PATH}`);

const PATHS = {
    frameworkHtml: join(FRAMEWORK_PATH, 'html'),  // Framework HTML/CSS/JS
    projectHtml: join(PROJECT_PATH, 'html'),      // Project-specific assets (logos, CSS, JS)
    dist: join(PROJECT_PATH, 'dist'),             // Output directory
    src: join(PROJECT_PATH, 'src/gulp')           // Generated headers directory
};

console.log('PATHS configuration:');
console.log(`  frameworkHtml: ${PATHS.frameworkHtml}`);
console.log(`  projectHtml: ${PATHS.projectHtml}`);
console.log(`  dist: ${PATHS.dist}`);
console.log(`  src: ${PATHS.src}`);

// Ensure the output directory exists
if (!existsSync(PATHS.src)) {
    console.log(`Creating missing directory: ${PATHS.src}`);
    mkdirSync(PATHS.src, { recursive: true });
}

// Recursively scan directory for files matching extension
function scanDirectory(dir, extension, baseDir = dir) {
    const files = [];

    if (!existsSync(dir)) {
        console.log(`  Directory does not exist: ${dir}`);
        return files;
    }

    const entries = readdirSync(dir);
    console.log(`  Scanning directory: ${dir} (${entries.length} entries)`);

    for (const entry of entries) {
        const fullPath = join(dir, entry);
        const stat = statSync(fullPath);

        if (stat.isDirectory()) {
            console.log(`    Found subdirectory: ${entry}`);
            files.push(...scanDirectory(fullPath, extension, baseDir));
        } else if (entry.endsWith(extension)) {
            // Store relative path from base directory for better organization
            const relativePath = relative(baseDir, fullPath);
            console.log(`    Found matching file: ${entry} -> ${relativePath}`);
            files.push(relativePath);
        } else {
            // console.log(`    Skipping file: ${entry} (doesn't end with ${extension})`);
        }
    }

    return files;
}

// Find project CSS and JS files
function findProjectAssets() {
    console.log(`\n=== Scanning for project assets ===`);
    console.log(`Project HTML path: ${PATHS.projectHtml}`);
    console.log(`Path exists: ${existsSync(PATHS.projectHtml)}`);

    const assets = {
        css: [],
        js: []
    };

    if (existsSync(PATHS.projectHtml)) {
        console.log(`Scanning for .css files...`);
        assets.css = scanDirectory(PATHS.projectHtml, '.css');

        console.log(`Scanning for .js files...`);
        assets.js = scanDirectory(PATHS.projectHtml, '.js');

        console.log(`\nFound ${assets.css.length} project CSS file(s):`);
        assets.css.forEach(file => console.log(`  - ${file}`));

        console.log(`Found ${assets.js.length} project JS file(s):`);
        assets.js.forEach(file => console.log(`  - ${file}`));
    } else {
        console.log(`Project HTML directory does not exist, skipping asset scan`);
    }

    console.log(`=== Asset scan complete ===\n`);

    return assets;
}

// Intelligently find logos in project and framework directories
function findLogos() {
    const logos = [];
    const logoPattern = /^logo(-.*)?\.png$/;

    // Check project html directory first
    if (existsSync(PATHS.projectHtml)) {
        const projectFiles = readdirSync(PATHS.projectHtml);
        projectFiles.forEach(file => {
            if (logoPattern.test(file)) {
                logos.push({ file, source: 'project' });
            }
        });
    }

    // Then check framework html directory
    if (existsSync(PATHS.frameworkHtml)) {
        const frameworkFiles = readdirSync(PATHS.frameworkHtml);
        frameworkFiles.forEach(file => {
            // Only add if not already found in project
            if (logoPattern.test(file) && !logos.some(logo => logo.file === file)) {
                logos.push({ file, source: 'framework' });
            }
        });
    }

    console.log(`Found ${logos.length} logo(s):`);
    logos.forEach(logo => {
        console.log(`  - ${logo.file} (from ${logo.source})`);
    });

    return logos.map(logo => logo.file);
}

const LOGOS = findLogos();
const PROJECT_ASSETS = findProjectAssets();

const HTML_MIN_OPTIONS = {
    removeComments: false,
    minifyCSS: true,
    minifyJS: true
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
        console.log(`Injected ${assets.css.length} CSS file(s) into <head>`);
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
        console.log(`Injected ${assets.js.length} JS file(s) before </body>`);
    }

    // Write the modified HTML back
    createWriteStream(htmlPath).end(html);
}

async function writeHeaderFile(source, destination, name) {
    return new Promise((resolve, reject) => {
        try {
            const wstream = createWriteStream(destination);
            const data = readFileSync(source);

            const hashSum = createHash('sha256');
            hashSum.update(data);
            const hex = hashSum.digest('hex');

            wstream.write(`#define ${name}_len ${data.length}\n`);
            wstream.write(`const char ${name}_sha[] = "${hex}";\n`);
            wstream.write(`const uint8_t ${name}[] = {`);

            for (let i = 0; i < data.length; i++) {
                if (i % 1000 === 0) wstream.write("\n");
                wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
                if (i < data.length - 1) wstream.write(',');
            }

            wstream.write('\n};');
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
    return deleteAsync([join(PATHS.dist, '*')], { force: true });
}

function buildInlineHtml() {
    return src(join(PATHS.frameworkHtml, '*.html'))
        .pipe(favicon({ src: PATHS.frameworkHtml }))
        .pipe(inline(INLINE_OPTIONS))
        .pipe(dest(PATHS.dist));
}

async function injectAssets() {
    const htmlPath = join(PATHS.dist, 'index.html');

    if (!existsSync(htmlPath)) {
        console.log('No HTML file found to inject assets into');
        return;
    }

    injectProjectAssets(htmlPath, PROJECT_ASSETS);
}

function minifyAndCompress() {
    return src(join(PATHS.dist, 'index.html'))
        .pipe(htmlmin(HTML_MIN_OPTIONS))
        .pipe(gzip())
        .pipe(dest(PATHS.dist));
}

async function embedHtml() {
    const source = join(PATHS.dist, 'index.html.gz');
    const destination = join(PATHS.src, 'index.html.gz.h');
    await writeHeaderFile(source, destination, 'index_html_gz');
}

function compressLogo(filename) {
    // Check project directory first, then framework directory
    let sourcePath;
    if (existsSync(join(PATHS.projectHtml, filename))) {
        sourcePath = join(PATHS.projectHtml, filename);
    } else if (existsSync(join(PATHS.frameworkHtml, filename))) {
        sourcePath = join(PATHS.frameworkHtml, filename);
    } else {
        throw new Error(`Logo file not found: ${filename}`);
    }

    return src(sourcePath)
        .pipe(gzip())
        .pipe(dest(PATHS.dist));
}

async function embedLogo(filename) {
    const source = join(PATHS.dist, `${filename}.gz`);
    const destination = join(PATHS.src, `${filename}.gz.h`);
    const safeName = filename.replace(/[^a-z0-9]/gi, '_') + '_gz';

    await writeHeaderFile(source, destination, safeName);
}

function createLogoTask(filename) {
    const compress = () => compressLogo(filename);
    const embed = () => embedLogo(filename);

    return series(compress, embed);
}

// ============================================================================
// Task Composition
// ============================================================================

const logoTasks = LOGOS.map(logo => createLogoTask(logo));
const buildAll = series(
    clean,
    buildInlineHtml,
    injectAssets,
    minifyAndCompress,
    embedHtml,
    ...logoTasks
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
    buildAll as default
};
