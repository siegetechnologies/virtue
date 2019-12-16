/*Create User
 *Get data from create user modal window,
 *Send it to the server,
 *Authorize list of roles to new user
 *Refresh Data*/
function createUser() {
	var username = document.getElementById('inputUser').value;
	var password = document.getElementById('inputPassword').value;
	var isAdmin = document.getElementById('inputPermissions').value;

	var roles = createAssignment($('#rlhs'));

	var data = JSON.stringify({
		"username": username,
		"password": password,
		"isAdmin": isAdmin
	});
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "php/create.php",
		data: data,
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (res) {
			var resp = JSON.parse(res);
			if (resp.success == true) {
				userRoleAuthorize(username, window.assign1);
				CloseModal();
			}
			refresh();
		}
	});
}

/*Create Role
 *Get data from create role modal window,
 *Send it to the server,
 *Authorize list of user to role
 *Refresh Data*/
function createRole() {
	var role = document.getElementById('inputRole').value;
	var ram = document.getElementById('inputRAM').value + "Gi";

	if (ram == "Gi") {
		ram = "1Gi"
	}

	var cpu = document.getElementById('inputCPU').value;

	if (cpu == "") {
		cpu = "1";
	}

	//var mounts = createArray(document.getElementById('inputMounts').value);
	var mounts = document.getElementById('inputMounts').value;
	var virtlet = $("#inputVirtlet").val();
	var sensors = createArray(document.getElementById('inputSensors').value);
	var filter = $("#inputFilter").val();

	var users = createAssignment($('#ulhs'));

	var apps = createAssignment($('#alhs'));

	var data = JSON.stringify({
		"roleID": role,
		"mounts": mounts,
		"isVirtlet": virtlet,
		"ramsize": ram,
		"cpucount": Number(cpu),
		"applicationIDs": apps,
		"sensors": sensors,
		"filtering": filter
	});
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "php/create.php",
		data: data,
		error: function (jqXHR, textStatus, errorThrown) {
		
					$( '#error' ).removeClass('d-none');
			if(assign2.length == 0)
				{
					$( '#errorMessage' )[0].innerHTML = "No Applications Selected";
				}
			else
				{
					$( '#errorMessage' )[0].innerHTML = "Invalid User Input";
				}
			
					
				
		},
		success: function (res) {
			var resp = JSON.parse(res);
			if (resp.roleID == role) {
				roleUserAuthorize(role, window.assign1);
				CloseModal();
			}
			refresh();
		}
	});
}

/*Role User Authorize
 *Authorize list of users to a role*/
function roleUserAuthorize(role, users) {
	for (var i = 0; i < users.length; i++) {
		var data = JSON.stringify({
			"Function": "Authorize",
			"username": users[i],
			"roleID": role
		});
		$.ajax({
			statusCode: {
				404: function () {
					alert("User " + i + "was not assigned the role");
				}
			},
			async: true,
			method: "POST",
			cache: false,
			url: "php/create.php",
			data: data,
			error: function (jqXHR, textStatus, errorThrown) {
				alert(errorThrown);
			},
			success: function (res) {

			}
		});
	}
}

/*Role User UnAuthorize
 *Unauthorizes list of users with a role*/
function roleUserUnAuthorize(role, users) {
	for (var i = 0; i < users.length; i++) {
		var data = JSON.stringify({
			"Function": "Unuthorize",
			"username": users[i],
			"roleID": role
		});
		$.ajax({
			async: true,
			method: "POST",
			cache: false,
			url: "php/create.php",
			data: data,
			error: function (jqXHR, textStatus, errorThrown) {
				alert(errorThrown);
			},
			success: function (res) {

			}
		});
	}
}

/*User Role Authorize
 *Authorizes a list of roles with a user*/
function userRoleAuthorize(user, roles) {
	for (var i = 0; i < roles.length; i++) {
		var data = JSON.stringify({
			"Function": "Authorize",
			"username": user,
			"roleID": roles[i]
		});
		$.ajax({
			async: true,
			method: "POST",
			cache: false,
			url: "php/create.php",
			data: data,
			error: function (jqXHR, textStatus, errorThrown) {
				alert(errorThrown);
			},
			success: function () {

			}
		});
	}
}

/*User Role UnAuthorize
 *UnAuthorizes a list of roles with a user*/
function userRoleUnAuthorize(user, roles) {
	for (var i = 0; i < roles.length; i++) {
		var data = JSON.stringify({
			"Function": "Unuthorize",
			"username": user,
			"roleID": roles[i]
		});
		$.ajax({
			async: true,
			method: "POST",
			cache: false,
			url: "php/create.php",
			data: data,
			error: function (jqXHR, textStatus, errorThrown) {
				alert(errorThrown);
			},
			success: function () {

			}
		});
	}
}

/*Create Application
 *Get data from create application modal window,
 *Send it to the server,
 *Refresh Data*/
function createApp() {
	var applicationID = document.getElementById('inputApplication').value;
	var type = $('#inputType').val();
	var typeS = "";
		if(type == 0)
			{
				typeS = "linux";
			}
		else if(type == 1)
			{
				typeS = "windows";
			}
	var install = document.getElementById('inputInstall').value;
	var postInstall = document.getElementById('inputPI').value;
	var launchCmd = document.getElementById('inputLC').value;
	var icon = document.getElementById('inputIcon').value;
	var desktop = document.getElementById('inputDesktop').value;

	//Create JSON
	var data = JSON.stringify({
		"applicationID": applicationID,
		"type": typeS,
		"install": install,
		"postInstall": postInstall,
		"launchCmd": launchCmd,
		"icon": icon,
		"desktop": desktop
	});
	
	$.ajax({//Send to server
		async: true,
		method: "POST",
		cache: false,
		url: "php/create.php",
		data: data,
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function () {
			CloseModal();
			refresh();
		}
	});
}

/*Create Array
 *Create array from string with comma delimiters
 *Returns Array*/
function createArray(string) {
	var i = 0;
	var c = 0;
	var array = new Array(string[i]);
	i++;
	while (string[i]) {
		if (string[i] == ',') {//if Comma move to next cell
			c++;
			i++;
			if (string[i] == ' ') {//get rid of spaces after commas if they exist
				i++;
			}
			array[c] = string[i]; //Add next character after comma
			i++;
		} else {
			array[c] += string[i]; //Else add char to cell
			i++;
		}

	}
	return array;
}


/*Create Assignment
 *Generates Array from list of obj's active children
 *Returns Array*/
function createAssignment(obj) {
	var children = obj.children();

	var size = children.length;
	var c = 0;
	var assignments = new Array();

	for (var i = 0; i < size; i++) {
		var list = children[i].classList;
		var visible = true;
		for (var v = 0; v < list.length; v++) {
			if (list[v] == "d-none") {
				visible = false;
				break;
			}

		}
		if (visible) {
			assignments[c] = children[i].innerHTML;
			c++;
		}
	}
	return assignments;
}
