/*Details Role
 	*Authorize Deauthorize List of users to a role*/
function detailsRole() {
	if (window.assign1.length != 0) {
		roleUserAuthorize($('#inputRole').val(), window.assign1);
	}
	if (window.deassign1.length != 0) {
		roleUserUnAuthorize($('#inputRole').val(), window.deassign1);
	}
}

/*Details User
 	*Authorize Deauthorize List of roles to a user*/
function detailsUser() {
	if (window.assign1.length != 0) {
		userRoleAuthorize($('#inputUser').val(), window.assign1);
	}
	if (window.deassign1.length != 0) {
		userRoleUnAuthorize($('#inputUser').val(), window.deassign1);
	}
	if ($('#inputPassword').val().length != 0) {
		changePass();
	}
}

/*Change Password
 	*If password field has value issue a change password command to the server*/
function changePass() {
	for (var i = 0; i < users.length; i++) {
		var data = JSON.stringify({
			"Function": "Pass",
			"username": $('#inputUser').val(),
			"password": $('#inputPassword').val()
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
