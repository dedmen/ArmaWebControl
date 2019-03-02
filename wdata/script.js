var playerNames = [];
var socket;
var watchEnabled = false;
var watchTimeoutID = null;

function onMessage(data) {
    //message('<p class="message">Received: '+data);
    var obj = JSON.parse(data);

    function processMessage(msg){
        if (msg.type == "playerlist") {
            playerNames = msg.players;
            updatePlayerlistCombo();
        }
        if ('watch' in msg) {
            $(msg.watch).html(hljs.highlight('sqf', msg.res).value);
            $($(msg.watch).attr("in")).css('background', '#fff');

            if (watchTimeoutID == null) {
                watchTimeoutID =  setTimeout(function(){
                    updateWatchFields();
                    watchTimeoutID = null;
                }, document.getElementById('watchTimeout').value * 1000);
            }

        }
    }

    if (obj.constructor === Array) {
        obj.forEach(processMessage);
    } else {
        processMessage(obj);
    }
}

function send() {
    var text = $('#text').val();

    if (text=="") {
        message('<p class="warning">Please enter a message');
        return ;
    }
    try{
        socket.send(text);
        message('<p class="event">Sent: '+text)

    } catch(exception) {
       message('<p class="warning">');
    }
    $('#text').val("");
}

function message(msg) {
  $('#chatLog').append(msg+'</p>');
}

function executeLocalScript() {
    var script = $('#execLocalScript').val();

    var msg = {
        type: "Exec",
        script: script
    }
    socket.send(JSON.stringify(msg));
}

function executeGlobalScript() {
    var script = $('#execGlobalScript').val();

    var msg = {
        type: "ExecFunc",
        fnc: "CBA_fnc_globalExecute",
        args: [
            -1,
            {
                code: script
            }
        ]
    }
    socket.send(JSON.stringify(msg));
}

function executeUnitScript() {
    var msg = {
        type: "ExecFunc",
        fnc: "ArmaWebControl_main_fnc_execOnPlayername",
        args: [
            $('#unitlist').val(),
            {
                code: $('#execUnitScript').val()
            }
        ]
    }
    socket.send(JSON.stringify(msg));
}

function executeFromUnitScript() {
    var msg = {
        type: "ExecFunc",
        fnc: "ArmaWebControl_main_fnc_execFromUnit",
        args: [
            $('#unitlist2').val(),
            {
                code: $('#execFUnitScript').val()
            }
        ]
    }
    socket.send(JSON.stringify(msg));
}

function refreshPlayerList() {
    var msg = {
        type: "getPlayerlist"
    }
    socket.send(JSON.stringify(msg));
}

function updatePlayerlistCombo() {
    $('#unitlist').empty();
    $('#unitlist2').empty();
    $.each(playerNames, function(i, p) {
        $('#unitlist').append($('<option></option>').val(p).html(p));
        $('#unitlist2').append($('<option></option>').val(p).html(p));
    });
}

function fillPresets(combo, list) {
    $(combo).empty();
    for (var property in list) {
        $(combo).append($('<option></option>').val(property).html(property));
    }
}

function loadPreset(combo, textbox, presetList, highlightBox){
    var presetName = $(combo).val()
    var code = presetList[presetName];

    $(textbox).val(code);
    $(highlightBox).html(hljs.highlight('sqf', code).value);
}

function updateWatchFields() {
    var tasks = [];
    document.querySelectorAll('#Watch textarea').forEach((block) => {
        if ($(block).val() != ""){
            var msg = {
                type: "Exec",
                watch: $(block).attr("out"),
                script: $(block).val()
            }
            tasks.push(msg);
            $(block).css('background', '#ccc');
        }
    });
    socket.send(JSON.stringify(tasks));
}

function updateWatchEnabled() {
    var isEnabled = document.getElementById('doUpdateWatch').checked;

    if (!isEnabled && watchTimeoutID != null) {
        clearTimeout(watchTimeoutID);
        watchTimeoutID = null;
    } else if (isEnabled && watchTimeoutID == null) {
        watchTimeoutID =  setTimeout(function(){
            updateWatchFields();
            watchTimeoutID = null;
        }, document.getElementById('watchTimeout').value * 1000);
    }
}


$(document).ready(function() {

    if (!("WebSocket" in window)) {
        $('#chatLog, input, button, #examples').fadeOut("fast");
        $('<p>Oh no, you need a browser that supports WebSockets.</p>').appendTo('#container');
    } else {
        //The user has WebSockets

        connect();

        function connect() {
       
            var host = "ws://localhost:8082/socket/server/startDaemon.php";

            try{
                socket = new WebSocket(host);

                message('<p class="event">Socket Status: '+socket.readyState);

                socket.onopen = function(){
                    message('<p class="event">Socket Status: '+socket.readyState+' (open)');
                }

                socket.onmessage = (msg) => onMessage(msg.data);

                socket.onclose = function(){
                    message('<p class="event">Socket Status: '+socket.readyState+' (Closed)');
                }

            } catch(exception) {
               message('<p>Error'+exception);
            }

            $('#text').keypress(function(event) {
                if (event.keyCode == '13') {
                  send();
                }
            });

            $('#disconnect').click(function(){
               socket.close();
            });

        }//End connect

    }//End else


    presets_Unit = Object.assign({}, presets_All, presets_Unit);
    presets_Local = Object.assign({}, presets_All, presets_Local);
    presets_Global = Object.assign({}, presets_All, presets_Global);

    fillPresets('#unitPreset', presets_Unit);
    fillPresets('#localPreset', presets_Local);
    fillPresets('#globalPreset', presets_Global);
    fillPresets('#unitPreset2', presets_FromUnit);
    updateWatchEnabled();
});

function openCity(evt, cityName) {
// Declare all variables
var i, tabcontent, tablinks;

// Get all elements with class="tabcontent" and hide them
tabcontent = document.getElementsByClassName("tabcontent");
for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
}

// Get all elements with class="tablinks" and remove the class "active"
tablinks = document.getElementsByClassName("tablinks");
for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
}

// Show the current tab, and add an "active" class to the button that opened the tab
document.getElementById(cityName).style.display = "block";
evt.currentTarget.className += " active";
} 