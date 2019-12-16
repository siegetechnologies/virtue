/*Process Login
 *Gets Login Form Data, sends it to PHP,
 *Error: display error,
 *Success: pass server response to switch function */
function process()
{
	var form = $("#frmlogin"); 
	var frm_data = $(form).serialize(); //Get form data
	$( '#invalid' ).addClass( 'd-none' );
	$( '#timeout' ).addClass( 'd-none' );

	
	$.ajax({//Send data to server
		async: true,
		method:"POST",
		cache: false,
		url: "php/login.php",
		data: { frm_data },
		error: function(jqXHR, textStatus, errorThrown)
		{
			$( '#invalid' ).removeClass( 'd-none' );
		},
		success: function(data) {  Switch(data) }
		}); 
}

/*Switch Function
 * Reads respnonse from server
 * Call loggedIn if credentials were valid
 * Else display error, credentials not valid*/
function Switch(result)
{ 
	if(result == "success"){
		loggedIn();
	}else if(result == "error"){
		invalid();
	}
}

/*LoggedIn
 * Load the configure page*/
function loggedIn()
{
	window.location.replace("configure.html");
}

/*Invalid
 *Display error,
 *User provided invalid credentials */
function invalid()
{
	$( '#invalid' ).removeClass( 'd-none' );
	return false;
}

