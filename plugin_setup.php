
<?
function returnIfExists($json, $setting) {
    if ($json == null) {
        return "";
    }
    if (array_key_exists($setting, $json)) {
        return $json[$setting];
    }
    return "";
}

function convertAndGetSettings() {
    global $settings;
        
    $cfgFile = $settings['configDirectory'] . "/plugin.serial-event.json";
    if (file_exists($cfgFile)) {
        $j = file_get_contents($cfgFile);
        $json = json_decode($j, true);
        return $json;
    }
    $j = "[]";
    return json_decode($j, true);
}

$pluginJson = convertAndGetSettings();
?>


<div id="global" class="settings">
<fieldset>
<legend>FPP Serial Event Config</legend>

<script>

var SerialDevices = new Array();
<?
foreach (scandir("/dev/") as $fileName) {
    if ((preg_match("/^ttySC?[0-9]+/", $fileName)) ||
        (preg_match("/^ttyACM[0-9]+/", $fileName)) ||
        (preg_match("/^ttyO[0-9]/", $fileName)) ||
        (preg_match("/^ttyAMA[0-9]+/", $fileName)) ||
        (preg_match("/^ttyUSB[0-9]+/", $fileName))) {
        echo "SerialDevices['$fileName'] = '$fileName';\n";
    }
}
if (is_dir("/dev/serial/by-id")) {
    foreach (scandir("/dev/serial/by-id") as $fileName) {
        if (strcmp($fileName, ".") != 0 && strcmp($fileName, "..") != 0) {
            $linkDestination = basename(readlink('/dev/serial/by-id/' . $fileName));
            echo "SerialDevices['serial/by-id/$fileName'] = '$fileName -> $linkDestination';\n";
        }
    }
}
?>

var serialEventConfig = <? echo json_encode($pluginJson, JSON_PRETTY_PRINT); ?>;


var uniqueId = 1;
var modelOptions = "";
function AddSerialEventItem(type) {
    var id = $("#serialeventTableBody > tr").length + 1;
    var html = "<tr class='fppTableRow";
    if (id % 2 != 0) {
        html += " oddRow'";
    }
    html += "'><td class='colNumber rowNumber'>" + id + ".</td><td><span style='display: none;' class='uniqueId'>" + uniqueId + "</span></td>";

    html += DeviceSelect(SerialDevices, "");
    
    html += "<td><input type='text' minlength='7' maxlength='15' size='15' class='descrip' /></td>";
    html += "<td><select class='conditiontype'>";
    html += "<option value='contains'";
    if(type == 'contains') {html += " selected ";}
    html += ">Contains</option><option value='startswith'";
    if(type == 'startswith') {html += " selected ";}
    html += ">Starts With</option><option value='endswith'";
    if(type == 'endswith') {html += " selected ";}
    html += ">Ends With</option><option value='regex'";
    if(type == 'regex') {html += " selected ";}
    html += ">Regex</option></select>";
    html += "<td><input type='text'  minlength='7' maxlength='15' size='15' class='conditionvalue' />";
    html += "</td><td><table class='fppTable' border=0 id='tableSerialCommand_" + uniqueId +"'>";
    html += "<td>Command:</td><td><select class='serialcommand' id='serialcommand" + uniqueId + "' onChange='CommandSelectChanged(\"serialcommand" + uniqueId + "\", \"tableSerialCommand_" + uniqueId + "\" , false, PrintArgsInputsForEditable);'><option value=''></option></select></td></tr>";
    html += "</table></td></tr>";
    //selected
    $("#serialeventTableBody").append(html);

    LoadCommandList($('#serialcommand' + uniqueId));

    newRow = $('#serialeventTableBody > tr').last();
    $('#serialeventTableBody > tr').removeClass('selectedEntry');
    DisableButtonClass('deleteEventButton');
    uniqueId++;

    return newRow;
}

function SaveSerialEventItem(row) {
    var ip = $(row).find('.ipaddress').val();
    var startchan = parseInt($(row).find('.startchan').val(),10);

    var plugnum = parseInt($(row).find('.plugnum').val(),10);
	var conditiontype = $(row).find('.conditiontype').val();

    var json = {
        "ip": ip,
        "startchannel": startchan,
        "condition": conditiontype,
        "plugnumber": plugnum
    };
    return json;
}

function SaveSerialEventItems() {
    var serialeventConfig = [];
    var i = 0;
    $("#serialeventTableBody > tr").each(function() {
        serialeventConfig[i++] = SaveSerialEventItem(this);
    });
    
    var data = JSON.stringify(serialeventConfig);
    $.ajax({
        type: "POST",
	url: 'api/configfile/plugin.serial-event.json',
        dataType: 'json',
        async: false,
        data: data,
        processData: false,
        contentType: 'application/json',
        success: function (data) {
           SetRestartFlag(2);
        }
    });
}


function RenumberRows() {
    var id = 1;
    $('#serialeventTableBody > tr').each(function() {
        $(this).find('.rowNumber').html('' + id++ + '.');
        $(this).removeClass('oddRow');

        if (id % 2 != 0) {
            $(this).addClass('oddRow');
        }
    });
}
function RemoveSerialEventItem() {
    if ($('#serialeventTableBody').find('.selectedEntry').length) {
        $('#serialeventTableBody').find('.selectedEntry').remove();
        RenumberRows();
    }
    DisableButtonClass('deleteEventButton');
}


$(document).ready(function() {
                  
    $('#serialeventTableBody').sortable({
        update: function(event, ui) {
            RenumberRows();
        },
        item: '> tr',
        scroll: true
    }).disableSelection();

    $('#serialeventTableBody').on('mousedown', 'tr', function(event,ui){
        $('#serialeventTableBody tr').removeClass('selectedEntry');
        $(this).addClass('selectedEntry');
        EnableButtonClass('deleteEventButton');
    });
});

</script>
<div>
<table border=0>
<tr><td colspan='2'>
        <input type="button" value="Save" class="buttons genericButton" onclick="SaveSerialEventItems();">
        <input type="button" value="Add" class="buttons genericButton" onclick="AddSerialEventItem('light');">
        <input id="delButton" type="button" value="Delete" class="deleteEventButton disableButtons genericButton" onclick="RemoveSerialEventItem();">
    </td>
</tr>
</table>

<div class='fppTableWrapper fppTableWrapperAsTable'>
<div class='fppTableContents'>
<table class="fppTable" id="serialeventTable"  width='100%'>
<thead><tr class="fppTableHeader"><th>#</th><th></th><th>Description</th><th>Condition Type</th><th>Condition Value</th><th>Command</th></tr></thead>
<tbody id='serialeventTableBody'>
</tbody>
</table>
</div>

</div>
<div>
<p>
<div class="col-auto">
        <div>
            <div class="row">
                <div class="col">
                    Last Messages:&nbsp;<input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();">
                </div>
            </div>
            <div class="row">
                <div class="col">
                    <pre id="lastMessages" style='min-width:150px; margin:1px;min-height:300px;'></pre>
                </div>
            </div>
        </div>
    </div>
<p>
</div>
</div>
<script>

$.each(serialeventConfig, function( key, val ) {
    var row = AddSerialEventItem(val["conditiontype"]);
    $(row).find('.ipaddress').val(val["ip"]);
    $(row).find('.startchan').val(val["startchannel"]);
    $(row).find('.plugnum').val(val["plugnumber"]);
});
</script>

</fieldset>
</div>
