/**
 * Writes compressed C arrays of data files (web interface)
 * How to use it?
 *
 * 1) Install Node 11+ and npm
 * 2) npm install
 * 3) npm run build
 *
 * If you change data folder often, you can run it in monitoring mode (it will recompile and update *.h on every file change)
 *
 * > npm run dev
 *
 * How it works?
 *
 * It uses NodeJS packages to inline, minify and GZIP files. See writeHtmlGzipped and writeChunks invocations at the bottom of the page.
 */

const fs = require("fs");
const packageJson = require("../package.json");

/**
 *
 */
function hexdump(buffer) {
    let lines = [];

    for (let i = 0; i < buffer.length; i += 16) {
        let block = buffer.slice(i, i + 16); // cut buffer into blocks of 16
        let hexArray = [];

        for (let value of block) {
            hexArray.push("0x" + value.toString(16).padStart(2, "0"));
        }

        let hexString = hexArray.join(", ");
        let line = `  ${hexString}`;
        lines.push(line);
    }

    return lines.join(",\n");
}

const inliner = require("inliner");
const zlib = require("zlib");

function strReplace(str, search, replacement) {
    return str.split(search).join(replacement);
}

function adoptVersionAndRepo(html) {
    let repoUrl = packageJson.repository ? packageJson.repository.url : undefined;
    if (repoUrl) {
        repoUrl = repoUrl.replace(/^git\+/, "");
        repoUrl = repoUrl.replace(/\.git$/, "");
        // Replace we
        html = strReplace(html, "https://github.com/atuline/WLED", repoUrl);
        html = strReplace(html, "https://github.com/Aircoookie/WLED", repoUrl);
    }

    let version = packageJson.version;
    if (version) {
        html = strReplace(html, "##VERSION##", version);
    }

    return html;
}

function writeHtmlGzipped(sourceFile, resultFile) {
    console.info("Reading " + sourceFile);
    new inliner(sourceFile, function(error, html) {
        console.info("Inlined " + html.length + " characters");
        html = filter(html, "html-minify-ui");
        console.info("Minified to " + html.length + " characters");

        if (error) {
            console.warn(error);
            throw error;
        }

        html = adoptVersionAndRepo(html);
        zlib.gzip(html, { level: zlib.constants.Z_BEST_COMPRESSION }, function(error, result) {
            if (error) {
                console.warn(error);
                throw error;
            }

            console.info("Compressed " + result.length + " bytes");
            const array = hexdump(result);
            const src = `/*
 * Binary array for the Web UI.
 * gzip is used for smaller size and improved speeds.
 * 
 */
 
// Autogenerated from ${sourceFile}, do not edit!!
const uint16_t PAGE_index_L = ${result.length};
const uint8_t PAGE_index[] PROGMEM = {
${array}
};
`;
            console.info("Writing " + resultFile);
            fs.writeFileSync(resultFile, src);
        });
    });
}

const CleanCSS = require("clean-css");
const MinifyHTML = require("html-minifier-terser").minify;
const UglifyJS = require("uglify-js");

function filter(str, type) {
    str = adoptVersionAndRepo(str);

    if (type === undefined) {
        return str;
    } else if (type == "css-minify") {
        return new CleanCSS({}).minify(str).styles;
    } else if (type == "html-minify") {
        return MinifyHTML(str, {
            collapseWhitespace: true,
            maxLineLength: 300,
            minifyCSS: true,
            minifyJS: true,
            continueOnParseError: false,
            removeComments: true,
        });
    } else if (type == "html-minify-ui") {
        return MinifyHTML(str, {
            collapseWhitespace: true,
            conservativeCollapse: true,
            maxLineLength: 300,
            minifyCSS: true,
            minifyJS: true,
            continueOnParseError: false,
            removeComments: true,
        });
    } else if (type == "js-minify") {
        return UglifyJS.minify(str).code;
    } else {
        console.warn("Unknown filter: " + type);
        return str;
    }
}

function specToChunk(srcDir, s) {
    if (s.method == "plaintext") {
        const buf = fs.readFileSync(srcDir + "/" + s.file);
        const str = buf.toString("utf-8");
        const chunk = `
// Autogenerated from ${srcDir}/${s.file}, do not edit!!
const char ${s.name}[] PROGMEM = R"${s.prepend || ""}${filter(str, s.filter)}${
      s.append || ""
    }";

`;
        return s.mangle ? s.mangle(chunk) : chunk;
    } else if (s.method == "binary") {
        const buf = fs.readFileSync(srcDir + "/" + s.file);
        const result = hexdump(buf);
        const chunk = `
// Autogenerated from ${srcDir}/${s.file}, do not edit!!
const uint16_t ${s.name}_SIZE = ${result.length};
const uint8_t ${s.name}[] PROGMEM = {
${result}
};

`;
        return s.mangle ? s.mangle(chunk) : chunk;
    } else {
        console.warn("Unknown method: " + s.method);
        return undefined;
    }
}

function writeChunks(srcDir, specs, resultFile) {
    let src = `/*
 * More web UI HTML source arrays.
 * This file is auto generated, please don't make any changes manually.
 */ 
`;
    specs.forEach((s) => {
        try {
            console.info("Reading " + srcDir + "/" + s.file + " as " + s.name);
            src += specToChunk(srcDir, s);
        } catch (e) {
            console.warn(
                "Failed " + s.name + " from " + srcDir + "/" + s.file,
                e.message.length > 60 ? e.message.substring(0, 60) : e.message
            );
        }
    });
    console.info("Writing " + src.length + " characters into " + resultFile);
    fs.writeFileSync(resultFile, src);
}

//writeHtmlGzipped("wled00/data/index.htm", "wled00/html_ui.h");
writeChunks(
    "html_gui", [{
            file: "index.htm",
            name: "PAGE_INDEX",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "html-minify",
            mangle: (str) =>
                str
                .replace("./bootstrap.bundle.min.js", "/bootstrap.bundle.min.js")
                .replace("./feather.min.js", "/feather.min.js")
                .replace("./bootstrap.min.css", "/bootstrap.min.css")
                .replace("./dashboard.css", "/dashboard.css")
                .replace("./jquery.min.js", "/jquery.min.js")
                .replace("./pixelit.js", "/pixelit.js")
        },
        {
            file: "config.htm",
            name: "PAGE_CONFIG",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "html-minify"
        },
        {
            file: "dash.htm",
            name: "PAGE_DASH",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "html-minify"
        },
        {
            file: "testarea.htm",
            name: "PAGE_TEST",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "html-minify"
        },
        {
            file: "update.htm",
            name: "PAGE_UPDATE",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "html-minify"
        },
        {
            file: "pixelit.js",
            name: "SCRIPT_JS_PIXELIT",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "bootbox.all.min.js",
            name: "SCRIPT_JS_BOOTBOX",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "bootstrap.bundle.min.js",
            name: "SCRIPT_JS_BOOTSTRAP",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "feather.min.js",
            name: "SCRIPT_JS_FEATHER",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "huebee.min.js",
            name: "SCRIPT_JS_HUEBEE",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "jquery.min.js",
            name: "SCRIPT_JS_JQUERY",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "site.js",
            name: "SCRIPT_JS_SITE",
            prepend: "=====(",
            append: ")=====",
            method: "plaintext",
            filter: "js-minify",
        },
        {
            file: "bootstrap.min.css",
            name: "STYLE_CSS_BOOTSTRAP",
            prepend: "=====(<style>",
            append: "</style>)=====",
            method: "plaintext",
            filter: "css-minify",
        },
        {
            file: "dashboard.css",
            name: "STYLE_CSS_DASHBOARD",
            prepend: "=====(<style>",
            append: "</style>)=====",
            method: "plaintext",
            filter: "css-minify",
        },
        {
            file: "huebee.min.css",
            name: "STYLE_CSS_HUEBEE",
            prepend: "=====(<style>",
            append: "</style>)=====",
            method: "plaintext",
            filter: "css-minify",
        }
    ],
    "include/Webinterface.h"
);