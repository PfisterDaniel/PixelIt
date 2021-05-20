var ipAddress = $(location).attr('hostname');
var pageName = 'dash';
var devMode = false;
var timeleft;
var rebootTimer;
var json;
var fwVersion;

if (ipAddress.includes('localhost') || ipAddress.includes('C:/')) {
    devMode = true;
}

$(function() {
    // Akive Menu Button select 
    $('.nav-link').click(function() {
        $('.nav-link').removeClass('active');
        $(this).addClass('active');
    })
    ChangePage("dash");
});

var connection = null;

function capitalizeFirstLetter(string) {
    return string.charAt(0).toUpperCase() + string.slice(1);
}

function connectionStart() {
    if (connection != null && connection.readyState != WebSocket.CLOSED) {
        connection.close();
    }

    var wsServer = ipAddress;
    // Dev Option
    if (devMode) {
        wsServer = '10.10.1.180'
    }
    connection = new WebSocket('ws://' + wsServer + ':81/' + pageName);
    connection.onopen = function() {
        $("#connectionStatus").html("Online");
        $("#connectionStatus").removeClass("text-danger");
        $("#connectionStatus").addClass("text-success");

        if (pageName == 'setConfig') {
            connection.send(json);
            connection.close();
        }

        //KeepAlive();
    }
    connection.onclose = function(e) {
        // Debug
        console.log('WebSocket connection close');
        $("#connectionStatus").html("Offline");
        $("#connectionStatus").removeClass("text-success");
        $("#connectionStatus").addClass("text-danger");
        /*
        if (pageName != 'config') {
            setTimeout(function () {
                connectionStart()
            }, 1000);
              }
              */
    }
    connection.onerror = function(error) {
        // Debug
        console.log('WebSocket Error ' + error);
        if (connection.readyState !== WebSocket.CLOSED) {
            connection.close();
        }
    }
    connection.onmessage = function(e) {
        // Debug
        console.log('WebSocket incomming message: ' + e.data);
        RefershData(e.data)
    }

    function KeepAlive() {
        var timeout = 1000;
        if (connection.readyState == WebSocket.OPEN) {
            connection.send("KeepAlive");
        }
        setTimeout(KeepAlive, timeout);
    }
}

function RefershData(input) {
    // Prüfen ob es ein Json ist
    if (!input.startsWith("{")) {
        return;
    }

    var json = $.parseJSON(input);
    // Log Json
    if (json.log && pageName == 'dash') {
        var logArea = $('#log');
        logArea.append("[" + json.log.timeStamp + "] " + json.log.function+": " + json.log.message + "\n");
        logArea.scrollTop(logArea[0].scrollHeight);

    } else {
        $.each(json, function(key, val) {
            // Config Json
            if (pageName == 'config') {
                if (typeof val === 'boolean') {
                    $("#" + key).prop('checked', val);
                } else {
                    $("#" + key).val(val.toString());
                }
            }
            // SystemInfo Json
            if (pageName == 'dash') {
                if (key == "firmwareVersion") {
                    fwVersion = val.toString();
                }
                $("#" + key).html(val.toString());
            }
        });
    }
}

function SaveConfig() {
    var obj = {};
    // Alle Inputs auslesen
    $("input").each(function() {
        // Debug
        console.log('SaveConfig -> ID: ' + this.id + ', Val: ' + (this.type == 'checkbox' ? $(this)
            .prop('checked') : $(this).val()));

        if (this.type == 'checkbox') {
            obj[this.id] = $(this).prop('checked');
        } else {
            obj[this.id] = $(this).val();
        }
    });

    // Alle Selects auslesen
    $("select").each(function() {
        // Debug
        console.log('SaveConfig -> ID: ' + this.id + ', Val: ' + $(this).val());
        obj[this.id] = $(this).val();
    });

    json = JSON.stringify(obj);
    // Debug
    console.log(json);

    pageName = "setConfig";

    connectionStart();

    // Restart Countdown usw.
    var timeout = 12000;
    StartCountDown(timeout / 1000);

    setTimeout(function() {
        $("#popup").modal('hide');
    }, timeout);

    setTimeout(function() {
        $("#dash").click();
    }, timeout + 500);
}

function ChangePage(_pageName) {
    pageName = _pageName;

    // Dev Option
    if (devMode) {
        $.ajax({
            type: 'GET',
            beforeSend: function(request) {
                request.setRequestHeader("Caller", "MyPixelDashboard");
            },
            url: "./" + _pageName + ".htm",
        }).done(function(data) {
            $("#mainContent").html(data);
        });
    } else {
        $.ajax({
            type: 'GET',
            beforeSend: function(request) {
                request.setRequestHeader("Caller", "MyPixelDashboard");
            },
            url: "./" + _pageName
        }).done(function(data) {
            $("#mainContent").html(data);
        });
    }

    if (_pageName == 'testarea') {
        pageName = 'setScreen';
    }
}

// Countdown
function StartCountDown(_timeleft) {
    timeleft = _timeleft;
    rebootTimer = setInterval(function() {
        timeleft--;
        $("#countdowntimer").html(timeleft);
        if (timeleft <= 0)
            clearInterval(rebootTimer);
    }, 1000);
}

function SendTest(type, input) {

    if (isNullOrWhitespace(input)) {
        return;
    }

    var obj = {};
    var scrollDelay = 200;
    switch (type) {
        case 'clock':
            obj["switchAnimation"] = {};
            obj["switchAnimation"]["aktiv"] = true;
            obj["switchAnimation"]["animation"] = "coloredBarWipe";
            obj["clock"] = {};
            obj["clock"]["show"] = true;
            obj["clock"]["switchAktiv"] = true;
            obj["clock"]["switchSec"] = 6;
            obj["clock"]["withSeconds"] = true;
            obj["color"] = {};
            obj["clock"]["r"] = 255;
            obj["clock"]["g"] = 255;
            obj["clock"]["b"] = 255;
            obj = JSON.stringify(obj);
            break;
        case 'brightness':
            obj["brightness"] = input;
            obj = JSON.stringify(obj);
            break;
        case 'text':

            if ($("#scrollTextDelayInput").val() > 0) {
                scrollDelay = $("#scrollTextDelayInput").val();
            }
            obj["text"] = {};
            obj["text"]["textString"] = input;
            obj["text"]["scrollText"] = "auto";
            obj["text"]["scrollTextDelay"] = scrollDelay;
            obj["text"]["bigFont"] = false;
            obj["text"]["centerText"] = false;
            obj["text"]["position"] = {};
            obj["text"]["position"]["x"] = 0;
            obj["text"]["position"]["y"] = 1;
            obj["text"]["color"] = {};
            obj["text"]["color"]["r"] = 255;
            obj["text"]["color"]["g"] = 255;
            obj["text"]["color"]["b"] = 255;
            obj = JSON.stringify(obj);
            break;
        case 'json':
            obj = input;
            break;
        case 'bitmap':
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
            break;
    }
    // Debug
    console.log(obj);
    connection.send(obj);
}

function isNullOrWhitespace(input) {

    if (typeof input === 'undefined' || input == null) {
        return true;
    }
    return input.replace(/\s/g, '').length < 1;
}