$(document).ready(function () {
	setVirtue();//Load virtue data immediatly after login
});

/*Refresh
 *Passes current window state variable into include function,
 *This gets the list of the currently selected data type,*/
function refresh() {
	include(window.state);
	$('#filter').val('');//Clear search filters value
}

/*RemovePrimary
 *Removes the blue coloring from all data type nav buttons*/
function removePrimary() {
	$('#virtuebtn').removeClass('btn-primary');
	$('#userbtn').removeClass('btn-primary');
	$('#rolebtn').removeClass('btn-primary');
	$('#appbtn').removeClass('btn-primary');
	resetPag();
}

/*resetPag
 *Removes pagination and clears search filter value
 *Legacy function, probably doesnt do anything visible*/
function resetPag() {
	document.getElementById('pagi').innerHTML = "";
	document.getElementById('filter').value = "";
}

/*Set Virtue
 *Sets Window state,
 *Assigns blue data type nav coloring
 *Gets List of Virtues*/
function setVirtue() {
	removePrimary();
	$('#virtuebtn').addClass('btn-primary');
	include("Virtue");
	$('#filter').val('');
	window.state = "Virtue";

}

/*Set User
 *Sets Window state,
 *Assigns blue data type nav coloring
 *Gets List of Users*/
function setUser() {
	removePrimary();
	$('#userbtn').addClass('btn-primary');
	include("User");
	$('#filter').val('');
	window.state = "User";
}

/*Set Role
 *Sets Window state,
 *Assigns blue data type nav coloring
 *Gets List of Roles*/
function setRole() {
	removePrimary();
	$('#rolebtn').addClass('btn-primary');
	include("Role");
	$('#filter').val('');
	window.state = "Role";
}

/*Set App
 *Sets Window state,
 *Assigns blue data type nav coloring
 *Gets List of Apps*/
function setApp() {
	removePrimary();
	$('#appbtn').addClass('btn-primary');
	include("App");
	$('#filter').val('');
	window.state = "App";
}

/*Include(dataType)
	*Requests a data type (mode) from the server,
	*Sends response to the includeTables switch for processing*/
function include(mode) {

	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "php/includes.php",
		data: {
			mode
		},
		error: function (jqXHR, textStatus, errorThrown) {
			window.location.replace("indexx.html");
		},
		success: function (data) {
			includeTables(data, mode);
		}
	});
}

/*Include Tables (data, DataType)
	*Switch Function
	*Sends data to the appropriate function 
	so that it may be rendered on the page*/
function includeTables(data, mode) {
	switch (mode) {
		case "Virtue":
			virtueTable(data, mode);
			break;
		case "User":
			userTable(data, mode);
			break;
		case "Role":
			roleTable(data, mode);
			break;
		case "App":
			appTable(data, mode);
			break;
		default:
			virtueTable(data, mode);
			break;
	}
}

/*VirtueTable(data, DataType)
	*Gets template virtue data page from server,
	*Sends the data nd parsed elements to 
	IncludeVirtueElements so they may be rendered*/
function virtueTable(data, mode) {
	data = JSON.parse(data);
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "includes/virtueTable.html",
		data: {
			mode
		},
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (htmldata) {
			var MenuLaunch = parseMenuLaunch(htmldata);
			var MenuAction = parseMenuAction(htmldata);
			var tHeader = parseTable(htmldata);
			var looper = parseLooper(htmldata);
			var filter = parseFilter(htmldata);
			includeVirtueElements(MenuLaunch, MenuAction, tHeader, filter, looper, data);
		}
	});
}

/*UserTable(data, DataType)
	*Gets template user data page from server,
	*Sends the data nd parsed elements to 
	IncludeUserElements so they may be rendered*/
function userTable(data, mode) {
	data = JSON.parse(data);
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "includes/userTable.html",
		data: {
			mode
		},
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (htmldata) {
			var MenuLaunch = parseMenuLaunch(htmldata);
			var MenuAction = parseMenuAction(htmldata);
			var tHeader = parseTable(htmldata);
			var looper = parseLooper(htmldata);
			var filter = parseFilter(htmldata);
			includeUserElements(MenuLaunch, MenuAction, tHeader, filter, looper, data);
		}
	});
}

/*RoleTable(data, DataType)
	*Gets template role data page from server,
	*Sends the data nd parsed elements to 
	IncludeRoleElements so they may be rendered*/
function roleTable(data, mode) {
	data = JSON.parse(data);
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "includes/RoleTable.html",
		data: {
			mode
		},
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (htmldata) {
			var MenuLaunch = parseMenuLaunch(htmldata);
			var MenuAction = parseMenuAction(htmldata);
			var tHeader = parseTable(htmldata);
			var looper = parseLooper(htmldata);
			var filter = parseFilter(htmldata);
			includeRoleElements(MenuLaunch, MenuAction, tHeader, filter, looper, data);
		}
	});
}

/*AppTable(data, DataType)
	*Gets template app data page from server,
	*Sends the data and parsed elements to 
	IncludeAppElements so they may be rendered*/
function appTable(data, mode) {
	data = JSON.parse(data);
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "includes/AppTable.html",
		data: {
			mode
		},
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (htmldata) {
			var MenuLaunch = parseMenuLaunch(htmldata);
			var MenuAction = parseMenuAction(htmldata);
			var tHeader = parseTable(htmldata);
			var looper = parseLooper(htmldata);
			var filter = parseFilter(htmldata);
			includeAppElements(MenuLaunch, MenuAction, tHeader, filter, looper, data);
		}
	});
}


/*GetScript
	*Returns the HTML between the start and end tags*/
function getScripts(data) {
	var start = data.indexOf("<includescripts>") + 6;
	var end = data.lastIndexOf("</includescripts>");
	return data.slice(start, end);
}

/*Parse Menu Launch
	*Returns the HTML between the start and end tags*/
function parseMenuLaunch(data) {
	var start = data.indexOf("<menuLaunch>") + 12;
	var end = data.indexOf("</menuLaunch>");
	var menu = data.slice(start, end);
	return menu;
}

/*Parse Menu Action
	*Returns the HTML between the start and end tags*/
function parseMenuAction(data) {
	var start = data.indexOf("<menuAction>") + 12;
	var end = data.indexOf("</menuAction>");
	var menu = data.slice(start, end);
	return menu;
}

/*Parse Filter
	*Returns the HTML between the start and end tags*/
function parseFilter(data) {
	var start = data.indexOf("<filter>") + 8;
	var end = data.indexOf("</filter>");
	var filter = data.slice(start, end);
	return filter;
}

/*Parse Table
	*Returns the HTML between the start and end tags*/
function parseTable(data) {
	var start = data.indexOf("<mtable>") + 8;
	var end = data.indexOf("</mtable>");
	return data.slice(start, end);
}

/*Parse Looper
	*Returns the HTML between the start and end tags*/
function parseLooper(data) {
	var start = data.indexOf("<looper>") + 9;
	var end = data.lastIndexOf("</looper>");
	return data.slice(start, end);
}

/*Reset Fields
	*Clears the Inner HTML of all page items
	*Used when changing datatype or pagaination page*/
function resetFields() {
	document.getElementById('pagi').innerHTML = "";
	document.getElementById("MenuLaunch").innerHTML = "";
	document.getElementById("MenuAction").innerHTML = "";
	document.getElementById('data').innerHTML = "";
	if (document.getElementById('infoTable')) {
		document.getElementById('infoTable').innerHTML = "";
	}
}

/*Setup Menu
	*Hides menu if data type does not have a CMD in that menu*/
function setupMenu(MenuLaunch, MenuAction, filter) {
	document.getElementById("MenuLaunch").innerHTML = MenuLaunch;
	
	if ($('#MenuLaunch').children().length == 1) {
		$('#MenuLaunch').addClass('d-none');
	} else {
		$('#MenuLaunch').addClass('custom-select').removeClass('btn').removeClass('btn-primary').removeClass('d-none');
	}

	document.getElementById("MenuAction").innerHTML = MenuAction;
	if ($('#MenuAction').children().length == 2) {
		$('#MenuAction').removeClass('custom-select').addClass('btn').addClass('btn-primary');
	} else if ($('#MenuAction').children().length == 1) {
		$('#MenuAction').addClass('d-none');
	} else {
		$('#MenuAction').addClass('custom-select').removeClass('btn').removeClass('btn-primary');
	}

}

/*Include Virtue Elements
	*Renders template data into page
	*Loops through data results adding a new row for each object
	*Sets up pagination if needed*/

function includeVirtueElements(MenuLaunch, MenuAction, tHeader, filter, looper, data) {
	resetFields(); //Clear page
	document.getElementById('page').value = 1; //Always start at page 1
	document.getElementById('mode').value = "Virtue"; 
	setupMenu(MenuLaunch, MenuAction, filter); //Show or hide menus based off usage

	document.getElementById('data').innerHTML = tHeader; //Add table header

	var r = document.getElementById("numResults").value; //Get setting of results combobox

	var s = data.length; //Length(or size) of data
	for (var i = 0; i < s; i++) { //Do once for each object

		var tbl = document.getElementById('infoTable');
		document.getElementById('infoTable').innerHTML += looper; //add looper element(premade row with classes assigned)

		addPagination(i, r, s); //Determines if new row is visible (on this page or not) adds page buttons to screen as needed

		var row = document.getElementById('null').childNodes; //Null ID is only on new looper objects

		row[3].innerHTML = Number(i) + 1; //Set row index
		row[5].innerHTML = data[i].virtueID; //set row virtue ID
		
		row[7].innerHTML = data[i].state; //set row state
		if (data[i].state == 'Restarting' | data[i].state == 'Starting') {
			document.getElementById('null').classList.add("bg-warning");
		} else if (data[i].state == 'Failed' | data[i].state == 'Stopped') {
			document.getElementById('null').classList.add("bg-danger");
			document.getElementById('null').classList.add("text-light");
		}
		row[9].innerHTML = data[i].userID; //Set row userID
		row[11].innerHTML = data[i].roleID;//Set Row roleID
		document.getElementById('null').id = "item-" + i; //Rename null ID
	}
}


/*Include User Elements
	*Renders template data into page
	*Loops through data results adding a new row for each object
	*Sets up pagination if needed*/
function includeUserElements(MenuLaunch, MenuAction, tHeader, filter, looper, data) {

	document.getElementById('page').value = 1;
	document.getElementById('mode').value = "User";

	setupMenu(MenuLaunch, MenuAction, filter);
	document.getElementById('data').innerHTML = tHeader;

	var r = document.getElementById("numResults").value;

	var s = data.users.length;
	for (var i = 0; i < s; i++) {

		var tbl = document.getElementById('infoTable');
		document.getElementById('infoTable').innerHTML += looper;

		addPagination(i, r, s);

		var row = document.getElementById('null').childNodes;

		row[3].innerHTML = Number(i) + 1;
		row[5].innerHTML = data.users[i].uname;

		if (data.users[i].isAdmin == 1) {
			row[7].innerHTML = 'Admin';
		} else if (data.users[i].isAdmin == 0) {
			row[7].innerHTML = 'User';
		}

		var l = data.users[i].roles.length;

		if (l == 0) {
			row[9].innerHTML = 'None';
		} else {
			for (var c = 0; c < l; c++) {
				row[9].innerHTML = data.users[i].roles[c].roleID + ', ';
			}
		}
		document.getElementById('null').id = "item-" + i;
	}
}

/*Include Role Elements
	*Renders template data into page
	*Loops through data results adding a new row for each object
	*Sets up pagination if needed*/
function includeRoleElements(MenuLaunch, MenuAction, tHeader, filter, looper, data) {

	document.getElementById('page').value = 1;
	document.getElementById('mode').value = "Roles";

	setupMenu(MenuLaunch, MenuAction, filter);
	document.getElementById('data').innerHTML = tHeader;

	var r = document.getElementById("numResults").value;

	var s = data.length;
	for (var i = 0; i < s; i++) {

		var tbl = document.getElementById('infoTable');
		document.getElementById('infoTable').innerHTML += looper;

		addPagination(i, r, s);

		var row = document.getElementById('null').childNodes;

		row[3].innerHTML = Number(i) + 1;
		row[5].innerHTML = data[i].roleID;
		row[7].innerHTML = data[i].repo;
		row[9].innerHTML = data[i].status;
		row[11].innerHTML = data[i].mounts;
		if (data[i].isVirtlet == 0) {
			row[13].innerHTML = 'No';
		} else {
			row[13].innerHTML = 'Yes';
		}
		if (data[i].ramsize == "") {
			row[15].innerHTML = "0";
		} else {
			row[15].innerHTML = data[i].ramsize;
		}
		row[17].innerHTML = data[i].cpucount;
		row[19].innerHTML = data[i].type;
		document.getElementById('null').id = "item-" + i;
	}
}

/*Include App Elements
	*Renders template data into page
	*Loops through data results adding a new row for each object
	*Sets up pagination if needed*/
function includeAppElements(MenuLaunch, MenuAction, tHeader, filter, looper, data) {

	document.getElementById('page').value = 1;
	document.getElementById('mode').value = "Apps";

	setupMenu(MenuLaunch, MenuAction, filter);
	document.getElementById('data').innerHTML = tHeader;

	var r = document.getElementById("numResults").value;

	var s = data.length;
	for (var i = 0; i < s; i++) {

		var tbl = document.getElementById('infoTable');
		document.getElementById('infoTable').innerHTML += looper;

		addPagination(i, r, s);

		var row = document.getElementById('null').childNodes;

		if (data[i].icon != "") {
			row[1].childNodes[3].childNodes[1].src = data[i].icon;
		} else {
			row[1].childNodes[3].childNodes[1].src = "https://via.placeholder.com/32";
		}
		row[1].childNodes[5].innerHTML = data[i].appID;
		row[3].childNodes[1].childNodes[3].childNodes[1].childNodes[3].innerHTML = data[i].launchCmd;
		row[3].childNodes[1].childNodes[3].childNodes[3].childNodes[3].innerHTML = data[i].install;
		row[3].childNodes[1].childNodes[3].childNodes[5].childNodes[3].innerHTML = data[i].icon;
		row[3].childNodes[1].childNodes[3].childNodes[7].childNodes[3].innerHTML = data[i].desktop;
		row[3].childNodes[1].childNodes[3].childNodes[9].childNodes[3].innerHTML = data[i].postInstall;
		row[3].childNodes[1].childNodes[3].childNodes[11].childNodes[3].innerHTML = data[i].type;

		document.getElementById('nulltbl').id = data[i].appID + "Collapse";
		document.getElementById('null').children[0].children[2].dataset.target = "#" + data[i].appID + "Collapse";

		document.getElementById('null').id = "item-" + i;
	}
}

/*Add Pagination
	*Adds pagination to data set if needed*/
function addPagination(i, r, s) {
	if (s > r) { //if display results is less than data size add pagination
		if (i >= r) //hide new item if not on current page
		{
			document.getElementById('null').classList.add("d-none"); //hide item
		}
		if (i == Number(r) + 1) //results + 1
		{
			var t = s / r;
			document.getElementById('pagi').classList.remove("d-none"); //show item
			paginate(t);//Setup bottom pag numbers

		}
	} else {
		$('.pagination').innerHTML = ""; //clear pagination it isnt needed
		$('.pagination').addClass('d-none'); //hide pagination
	}

}
