$(document).ready(function () {
	ModalReload(); //Allows for accessing modal elements
	window.assign1 = []; //Assignment list
	window.deassign1 = []; //Unassignment List


});

/*Modal Reload
 *Code run when modal window is first opened*/
function ModalReload() {
	window.page = 0;
	window.selection = [];


	/*Add to Selection from left column
	 *On click toggle color change
	 *Clear selection from right column*/
	$('.list').on("click", ".li", function (event) {
		$('.ri').removeClass('bg-primary');
		$('.ri').removeClass('text-white');
		$(this).toggleClass('bg-primary');
		$(this).toggleClass('text-white');
	});

	/*Add to Selection from right column
	 *On click toggle color change
	 *Clear selection from left column*/
	$('.list').on("click", ".ri", function (event) {
		$('.li').removeClass('bg-primary');
		$('.li').removeClass('text-white');
		$(this).toggleClass('bg-primary');
		$(this).toggleClass('text-white');
	});


	/*Next Modal Page
	 *Detects click on Next modal button,
	 *Toggles active state of next and previous buttons,
	 *Displays next data object in form*/
	$('.modal-footer').on("click", "#dnext", function (event) {
		window.page++;
		if (window.page > window.selection.length) {
			window.page--;
		} else {
			if (window.page == window.selection.length - 1) {
				$('#dnext').prop('disabled', true);
			}
			$('#dprev').prop('disabled', false);
			populateDetails(window.state);
		}
	})

	/*Previous Modal Page
	 *Detects click on previous modal button,
	 *Toggles active state of next and previous buttons,
	 *Displays previous data object in form*/
	$('.modal-footer').on("click", "#dprev", function (event) {
		window.page--;
		if (window.page < 0) {
			window.page++;
		} else {
			if (window.page == 0) {
				$('#dprev').prop('disabled', true);
			}
			$('#dnext').prop('disabled', false);
			populateDetails(window.state);
		}
	})

	/*Reset Values on modal Close
	 *Changes index on menu comboboxes,
	 *Clears the contents of the modal window*/
	$('#inputModal').on('hidden.bs.modal', function (e) {
		$('#MenuLaunch').val(0).change();
		$('#MenuAction').val(0).change();
		$('#inputModalContents').empty();
	})

	$('#inputModal').on('shown.bs.modal', function (e) {
		populate();
	})

	/*Get App Icon
	 *Gets URL from the Icon text field,
	 *Attempts to get the icon through Internet
	 *If Icon Exists, display it
	 *Else get default icon, display that*/
	$('#inputIcon').focusout(function () {

		var url = $(this).val();

		if (url == '') {
			noImg(); //Get default image
		} else {

			$.ajax({
				statusCode: {
					404: function () {
						noImg()
					}
				},
				async: true,
				method: "get",
				cache: false,
				url: url,
				error: function (jqXHR, textStatus, errorThrown) {

				},
				success: function (mbool) {

					$('#appIcon').attr("src", url).removeClass('d-none');
					$('#noIcon').addClass('d-none');


				}
			});
		}
	});

	/*Toggle OS Icon
	 *Switches OS Icon when desktop combobox changes values*/
	$('#inputType').on('change', function () {
		switch (this.value) {
			case "0":
				$('#typeIcon').addClass('fa-linux').removeClass('fa-windows');
				break;
			case "1":
				$('#typeIcon').removeClass('fa-linux').addClass('fa-windows');
				break;
		}
	});


	/*Column Sort Caret aniamtion
	 *Changes column caret orrientation on click*/
	$('.Trigger').click(function () {
		$(this).children('i').toggleClass('fa-caret-down').toggleClass('fa-caret-up');
	});

	/*Assign Role
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#assignRole').click(function () {
		var selected = $('#rrhs').children('.bg-primary');
		for (var i = 0; i < selected.length; i++) {
			deassign1.remove(selected[i].textContent);
			assign1.push(selected[i].textContent);
		}
		assign(selected);
	});

	/*UnAssign Role
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#unassignRole').click(function () {
		var selected = $('#rlhs').children('.bg-primary');
		for (var i = 0; i < selected.length; i++) {
			assign1.remove(selected[i].textContent);
			deassign1.push(selected[i].textContent);
		}
		unassign(selected);

	});

	/*Assign Users
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#assignUsers').click(function () {
		var selected = $('#urhs').children('.bg-primary');
		for (var i = 0; i < selected.length; i++) {
			deassign1.remove(selected[i].textContent);
			assign1.push(selected[i].textContent);
		}
		assign(selected);
	});

	/*UnAssign Users
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#unassignUsers').click(function () {
		var selected = $('#ulhs').children('.bg-primary');
		for (var i = 0; i < selected.length; i++) {
			assign1.remove(selected[i].textContent);
			deassign1.push(selected[i].textContent);
		}
		unassign(selected);

	});

	/*Assign Apps
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#assignApps').click(function () {
		var selected = $('#arhs').children('.bg-primary');
		assign(selected);
	});

	/*UnAssign Apps
	 *moves selected items from the right column to the left,
	 *adds or removes from assign design where appropriate*/
	$('#unassignApps').click(function () {
		var selected = $('#alhs').children('.bg-primary');
		unassign(selected);

	});

}

//Array Prototype, remove data that equals string
Array.prototype.remove = function () {
	var what, a = arguments,
		L = a.length,
		ax;
	while (L && this.length) {
		what = a[--L];
		while ((ax = this.indexOf(what)) !== -1) {
			this.splice(ax, 1);
		}
	}
	return this;
};

/*UnAssign
 *Removes elements from Assignment List
 *If not in assignment list add to UnAsignment List*/
function unassign(selected) {
	for (var i = 0; i < selected.length; i++) {
		var id = selected[i].id;
		$('#' + id).removeClass('bg-primary').removeClass('text-white').addClass('d-none').removeClass('d-block');
		if (id != null && id.length > 0 && id.charAt(id.length - 1) == 'l') {
			id = id.substring(0, id.length - 1);
			id += "r";
		}
		$('#' + id).removeClass('d-none').addClass('d-block').addClass('text-white').addClass('bg-primary').val('0');
	}
}

/*Assign
 *Removes elements from UnAssignment List
 *Adds element to Asignment List*/
function assign(selected) {
	for (var i = 0; i < selected.length; i++) {
		var id = selected[i].id;
		$('#' + id).removeClass('bg-primary').removeClass('text-white').addClass('d-none').removeClass('d-block');
		if (id != null && id.length > 0 && id.charAt(id.length - 1) == 'r') {
			id = id.substring(0, id.length - 1);
			id += "l";
		}
		$('#' + id).removeClass('d-none').addClass('d-block').addClass('text-white').addClass('bg-primary').val('1');
	}
}

/*No Img
 *Hide App Image with broken link
 *Show default App Image*/
function noImg() {
	$('#appIcon').attr("src", '').addClass('d-none');
	$('#noIcon').removeClass('d-none');
}

/*Populate
 *Omni Switch for populating modal window data,
 *Create appropriate variables and based off the data type and action performed by user
 *Gets Secondary data associated with selection from server*/
function populate() {
	switch (window.action) //Button pushed
	{
		case 'create':
			if (window.state == 'User') {
				window.data = [];
				window.roles = [];
				var outputs = [];
				outputs.push(window.data);
				outputs.push(window.roles);
				var gets = [];
				gets.push("Role");
				getData(gets, 0, outputs);
			} else if (window.state == 'Role') {
				window.applications = [];
				window.users = [];
				window.data = [];
				var outputs = [];
				outputs.push(window.data);
				outputs.push(window.users);
				outputs.push(window.applications);
				var gets = [];
				gets.push("User");
				gets.push("App");
				getData(gets, 0, outputs);
			}
			break;
		case 'import':
			populateImport(window.state);
			break;
		case 'details':
			if (window.state == 'App') {
				var selection = $('.card-header').children('input:checked').parent();
				for (var i = 0; i < selection.length; i++) {
					window.selection.push(selection[i].children[2].innerHTML); //ID
					if (i == 1) {
						$('#dnext').prop('disabled', false);
					}
				}
			} else {
				var selection = $('tbody').children('tr').children('td').children('input:checked').parent().parent(); //ID
				for (var i = 0; i < selection.length; i++) {
					if (i == 1) {
						$('#dnext').prop('disabled', false);
					}
					var row = selection[i].children;
					var item = [];
					for (var c = 2; c < row.length; c++) {
						item.push(row[c].innerText);
					}
					window.selection.push(item);

				}
			}



			populateDetails(window.state);
			break;
	}
	window.action = '';
}


/*Populate Details
 *Omni switch for Getting selection data
 *Path dependent on selected data type
 *Gets Primary data from the server for the selected data type*/
function populateDetails(state) {
	switch (state) {

		case 'Virtue':
			var virtueID = window.selection[window.page][0];
			window.data = [];
			var outputs = [];
			var gets = [];
			outputs.push(window.data);

			getDetails(virtueID, gets, outputs);
			break;
		case 'User':
			window.data = [];
			window.roles = [];
			var userID = window.selection[window.page][0];
			var outputs = [];
			outputs.push(window.data);
			outputs.push(window.roles);
			var gets = [];
			gets.push("Role");
			getDetails(userID, gets, outputs);
			break;
		case 'Role':
			window.applications = [];
			window.users = [];
			window.data = [];
			var roleID = window.selection[window.page][0];
			var outputs = [];
			outputs.push(window.data);
			outputs.push(window.users);
			outputs.push(window.applications);
			var gets = [];
			gets.push("User");
			gets.push("App");

			getDetails(roleID, gets, outputs);

			break;
		case 'App':
			window.data = [];
			window.roles = [];
			var appID = window.selection[window.page];
			var outputs = [];
			outputs.push(window.data);
			outputs.push(window.roles);
			var gets = [];
			gets.push("Role");
			getDetails(appID, gets, outputs);

			break;
	}
}

/*Populate User
 *Fills out form with data for the given page*/
function populateUser(page) {
	var item = window.selection[page];
	$('#inputUser').val(item[0]);
	if (item[1] == 'Admin') {
		$('#inputPermissions').val(1);
	} else {
		$('#inputPermissions').val(0);
	}

	var lhs = '';
	var rhs = '';
	for (var i = 0; i < window.data.length; i++) {
		lhs += "<div id='" + window.data[i][0].roleID + "-l' class='list-group-item d-none li'>" + window.data[i][0].roleID + "</div>";
		rhs += "<div id='" + window.data[i][0].roleID + "-r' class='list-group-item ri'>" + window.data[i][0].roleID + "</div>";
	}
	$('#rlhs').html(lhs);
	$('#rrhs').html(rhs);
}

/*Populate Import
 *Fills out title and label for the Import form*/
function populateImport(state) {
	document.getElementById('ModalLongTitle').innerHTML = 'Import ' + state;
	document.getElementById('jsonlabel').innerHTML = state + ' JSON:<span class="text-danger">';
}

/*Get Details
 *Gets Primary data for the first selected object
 *Success, add selection to List, Populates form
 *Calls recursive function getData for secondary data
 *Primary and secondary data are split to reduce visible lag*/
function getDetails(ID, gets, outputs) {
	var jdata = JSON.stringify({
		"Mode": window.mode.value,
		"ID": ID
	});
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "php/get.php",
		data: jdata,
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (data) {
			outputs[0].push(JSON.parse(data));
			populateTop(page);
			getData(gets, 0, outputs);
		}
	});
}

/*Get Data
 *Get full list of secondary data to accompany the selected data type
 *Recursively get more data until the end of the gets list is hit
 *Call populate bottom of the page*/
function getData(gets, i, outputs) {
	if (i >= gets.length) {
		populateBottom(page);
	} else {
		var mode = gets[i];
		$.ajax({
			async: true,
			method: "POST",
			cache: false,
			url: "php/includes.php",
			data: {
				mode
			},
			error: function (jqXHR, textStatus, errorThrown) {
				alert(errorThrown);
			},
			success: function (data) {
				i++;
				outputs[i].push(JSON.parse(data));
				getData(gets, i, outputs);

			}
		});
	}
}

/*Populate Top
 *Switch to guid the data into the right form*/
function populateTop(page) {
	switch (window.state) {
		case "Virtue":
			populateVirtueTop(page);
			break;
		case "User":
			populateUserTop(page);
			break;
		case "Role":
			populateRoleTop(page);
			break;
		case "App":
			populateAppTop(page);
			break;

	}
}

/*Populate Bottom
 *Switch to guid the data into the right form*/
function populateBottom(page) {
	switch (window.state) {
		case "Virtue":
			populateVirtueBottom(page);
			break;
		case "User":
			populateUserBottom(page);
			break;
		case "Role":
			populateRoleBottom(page);
			break;
		case "App":
			populateAppBottom(page);
			break;

	}
}

/*Populate Role Top
 *Populates the role detail modal with the currently selected Primary data*/
function populateRoleTop(page) {
	var item = window.data[0];
	$('#inputRole').val(item.roleID);
	var ramv = "";
	var ramt = "";
	for (var i = 0; i < item.ramsize.length; i++) {
		if (item.ramsize[i] <= 9 || item.ramsize[i] >= 0) {
			ramv += item.ramsize[i];
		} else {
			ramt += item.ramsize[i];
		}
	}

	$('#inputRAM').val(ramv);
	$('#ramType').innerHTML = ramt;
	
	$('#inputCPU').val(item.cpucount);
	$('#inputMounts').val(item.mounts);

	$('#inputVirtlet').val(item.isVirtlet);
	$('#inputStatus').val(item.status);
	if (item.type.toLowerCase() == "linux") {
		$('#inputType').val(0);
	} else if (item.type.toLowerCase() == "windows") {
		$('#inputType').val(1);
	}

}


/*Populate Role Bottom
 *Parses List of Secondary data to make a new list of data that is associated with the current selection
 *Populates the role detail modal with the currently selected Secondary data*/
function populateRoleBottom(page) {
	var item = window.data[0];

	$('#urhs').html('');
	$('#ulhs').html('');

	for (var i = 0; i < window.users[0].users.length; i++) {
		if (item == undefined) {
			$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-l' class='list-group-item li d-none'>" + window.users[0].users[i].uname + "</div>").appendTo('#ulhs');

			$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-r' class='list-group-item ri'>" + window.users[0].users[i].uname + "</div>").appendTo('#urhs');
		} else {
			for (var c = 0; c < window.users[0].users[i].roles.length; c++) {
				if (window.users[0].users[i].roles[c].roleID == item.roleID) {
					$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-l' class='list-group-item li'>" + window.users[0].users[i].uname + "</div>").appendTo('#ulhs');

					$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-r' class='list-group-item ri d-none'>" + window.users[0].users[i].uname + "</div>").appendTo('#urhs');

					break;

				} else if (c == window.users[0].users[i].roles.length - 1) {
					$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-l' class='list-group-item li d-none'>" + window.users[0].users[i].uname + "</div>").appendTo('#ulhs');

					$("<div id='" + window.users[0].users[i].uname.replace(/\s+/g, '') + "-r' class='list-group-item ri'>" + window.users[0].users[i].uname + "</div>").appendTo('#urhs');
				}
			}

		}
	}


	for (var i = 0; i < window.applications[0].length; i++) {
		if (item == undefined || item.applications.length == 0) {
			$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-l' class='list-group-item li d-none'>" + window.applications[0][i].appID + "</div>").appendTo('#alhs');

			$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-r' class='list-group-item ri'>" + window.applications[0][i].appID + "</div>").appendTo('#arhs');
		} else {
			for(var c = 0; c < item.applications.length; c++)
				{
					if(item.applications[c].appID == window.applications[0][i].appID)
						{
								$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-l' class='list-group-item li'>" + window.applications[0][i].appID + "</div>").appendTo('#alhs');

								$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-r' class='list-group-item ri  d-none'>" + window.applications[0][i].appID + "</div>").appendTo('#arhs');
						}
						else
							{
								$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-l' class='list-group-item li  d-none'>" + window.applications[0][i].appID + "</div>").appendTo('#alhs');

								$("<div id='" + window.applications[0][i].appID.replace(/\s+/g, '') + "-r' class='list-group-item ri '>" + window.applications[0][i].appID + "</div>").appendTo('#arhs');
							}
				
				}
		

		}
	}
}

/*Populate User Details form, Top
 *Populates the User name and permission level*/
function populateUserTop(page) {
	var item = window.data[0];
	$('#inputUser').val(item.uname);
	$('#inputPermissions').val(item.isAdmin);
}

/*Populate User Details form, Bottom
 *Only populates role field*/
function populateUserBottom(page) {
	var item = window.data[0];

	$('#rrhs').html('');
	$('#rlhs').html('');

	for (var i = 0; i < window.roles[0].length; i++) {

		if (item == undefined || item.roles.length == 0) {
			$("<div id='" + window.roles[0][i].roleID + "-l' class='list-group-item li d-none'>" + window.roles[0][i].roleID + "</div>").appendTo('#rlhs');

			$("<div id='" + window.roles[0][i].roleID + "-r' class='list-group-item ri'>" + window.roles[0][i].roleID + "</div>").appendTo('#rrhs');
		} else {
			
			for (var c = 0; c < item.roles.length; c++) {
				
					if (item.roles[c].roleID == window.roles[0][i].roleID) {
						$("<div id='" + window.roles[0][i].roleID + "-l' class='list-group-item li'>" + window.roles[0][i].roleID + "</div>").appendTo('#rlhs');

						$("<div id='" + window.roles[0][i].roleID + "-r' class='list-group-item ri d-none'>" + window.roles[0][i].roleID + "</div>").appendTo('#rrhs');

						break;

					} else if (c == item.roles.length - 1) {
						$("<div id='" + window.roles[0][i].roleID + "-l' class='list-group-item li d-none'>" + window.roles[0][i].roleID + "</div>").appendTo('#rlhs');

						$("<div id='" + window.roles[0][i].roleID + "-r' class='list-group-item ri'>" + window.roles[0][i].roleID + "</div>").appendTo('#rrhs');
					}
			
			}
			
		}

	}
}

/*Populate virtue Details Form, Top
 *Populates object details into the Virtue Details form
 *Does not populate Apps fields*/
function populateVirtueTop(page) {
	var item = window.data[0][0];
	$('#inputVirtue').val(item.virtueID);
	$('#inputUser').val(item.userID);
	$('#inputRole').val(item.roleID);
	$('#inputState').val(item.state);
	$('#inputHost').val(item.host);
	$('#inputPort').val(item.port);
	$('#inputRole').val(item.roleID);
	if (item.type.toLowerCase() == "linux") {
		$('#inputType').val(0);
	} else if (item.type.toLowerCase() == "windows") {
		$('#inputType').val(1);
	}
}

/*Populate virtue Details Form, Bottom
 *Only populates virtue Apps*/
function populateVirtueBottom(page) {
	var item = window.data[0][0];

	$('#vrhs').html('');

	for (var i = 0; i < item.applications.length; i++) {
		$("<div id='" + item.applications[i].appID + "-r' class='list-group-item ri'>" + item.applications[i].appID + "</div>").appendTo('#vrhs');

	}

}

/*Populate App Details Form, Top
 *Populates object details into the App Details form
 *Does not populate Role fields*/
function populateAppTop(page) {
	var item = window.data[0];
	$('#inputApplication').val(item.appID);
	if (item.type.toLowerCase() == "linux") {
		$('#inputType').val(0);
	} else if (item.type.toLowerCase() == "windows") {
		$('#inputType').val(1);
	}
	$('#inputInstall').val(item.install);
	$('#inputPI').val(item.postInstall);
	$('#inputLC').val(item.launchCMD);

	$('#inputIcon').val(item.Icon);
	$('#inputDesktop').val(item.desktop);

}


/*Populate App Details Form, Bottom
 *Only populates Role fields*/
function populateAppBottom(page) {
	var item = window.roles[0];

	$('#rrhs').html('');

	for (var i = 0; i < item.length; i++) {
		for (var c = 0; c < item[i].applications.length; c++) {
			if (item[i].applications[c].appID == window.data[0].appID) {
				$("<div id='" + item[i].roleID + "-r' class='list-group-item ri'>" + item[i].roleID + "</div>").appendTo('#rrhs');
				break;
			}


		}

	}

}

/*Remove Items
 *Sends a remove command to the server for each object in selection*/
function removeItems() {
	var mode = window.state;

	if (window.state == 'App') {
		var selection = $('.card-header').children('input:checked').parent();
	} else {
		var selection = $('tbody').children('tr').children('td').children('input:checked').parent().parent();
	}
	var length = selection.length;


	for (var c = 0; c < length; c++) {
		var id = selection[c].children[2].innerText;


		var jdata = JSON.stringify({
			"Mode": "remove" + mode,
			"ID": id
		});
		$.ajax({
			async: true,
			method: "POST",
			cache: false,
			url: "php/get.php",
			data: jdata,
			error: function (jqXHR, textStatus, errorThrown) {

			},
			success: function (msg) {
				var json = JSON.parse(msg);
				if (json.success == true) {
					CloseModal();
				} else if (json.error == 'There are some roles that use the application') {
					$('#error').removeClass('d-none');
					$('#errorMessage')[0].innerHTML = json.error + "<br>";
					$('#errorMessage')[0].innerHTML += "Roles: " + json.badroles;
				}
			}
		});
	}
	include(window.state);
}

/*Import Items
 *Collect JSON from the UI,
 *Pasres JSON to ensure it is valid,
 *Sends original JSON to server for importing*/
function importItems() {
	var mode = window.state;
	var text = $('#inputJSON').val();
	try {
		var json = JSON.parse(text);
	} catch (err) {
		$('#errorMessage')[0].innerText = err;
		$('#error').removeClass('d-none');
	}

	var jdata = JSON.stringify({
		"Mode": "import" + mode,
		"data": text
	});
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "php/get.php",
		data: jdata,
		error: function (jqXHR, textStatus, errorThrown) {

		},
		success: function () {
			include(window.state);
		}
	});
}
