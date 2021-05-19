var connection;

function RGB565IntArrayPaint() {
    var input = $("#output").val();

    if ($("#livedraw").prop('checked')) {
        LiveDraw(input);
    }

    input = input.replace("[", "").replace("]", "");
    var inputArray = input.split(",");
    var counter = 0;
    $(".pixel").each(function() {
        var rgb = RGB565IntToRGB(inputArray[counter]);
        $(this).css('background-color', RGBToHEX(rgb[0], rgb[1], rgb[2]));
        counter++;
    });
}

function CreateOutput(output) {
    if (output.endsWith(",")) {
        output = output.substr(0, output.length - 1);
    }
    $("#output").val("[" + output + "]");

    if ($("#livedraw").prop('checked')) {
        LiveDraw($("#output").val());
    }
}

function RGBToHEX(red, green, blue) {
    var rgb = blue | (green << 8) | (red << 16);
    return '#' + (0x1000000 + rgb).toString(16).slice(1)
}

function RGB565IntToRGB(color) {
    var r = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
    var g = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
    var b = (((color & 0x1F) * 527) + 23) >> 6;
    return [r, g, b];
}

function CreateRGBSplit(color) {
    color = color.replace("rgb(", "");
    color = color.replace(" ", "");
    color = color.replace(" ", "");
    color = color.replace(" ", "");
    color = color.replace(")", "");
    var rgbArray = color.split(",");
    return (rgbArray);
}

function RGB888ToRGB565(rgbArray) {
    return (((rgbArray[0] & 0xf8) << 8) + ((rgbArray[1] & 0xfc) << 3) + (rgbArray[2] >> 3));
}

function ConvertHexToInt() {
    input = $("#output").val();
    input = input.replace("(", "");
    input = input.replace(")", "");
    var array = input.split(",");
    newString = "";
    array.forEach(function(x) {
        var temp = x.replace(" ", "");
        newString += parseInt(temp, 16) + ",";
    });
    CreateOutput(newString);
}

function LiveDraw(input) {
    var obj = {};

    if (input.includes('],[')) {

        if (!input.startsWith('[[')) {
            input = '[' + input + ']';
        }

        obj["bitmapAnimation"] = {};
        obj["bitmapAnimation"]["data"] = input;
        obj["bitmapAnimation"]["animationDelay"] = 200;
    } else {
        obj["bitmap"] = {};
        obj["bitmap"]["data"] = input;
        obj["bitmap"]["position"] = {};
        obj["bitmap"]["position"]["x"] = 0;
        obj["bitmap"]["position"]["y"] = 0;
        obj["bitmap"]["size"] = {};
        obj["bitmap"]["size"]["height"] = 8;

        if (input.split(",").length == 64) {
            obj["bitmap"]["size"]["width"] = 8;
        } else {
            obj["bitmap"]["size"]["width"] = 32;
        }
    }
    obj = JSON.stringify(obj);
    obj = obj.replace("\"[", "[");
    obj = obj.replace("]\"", "]");

    // Debug
    console.log(obj);
    connection.send(obj);
}

$('input[type=radio]').on('change', function() {
    $(this).closest("form").submit();
});