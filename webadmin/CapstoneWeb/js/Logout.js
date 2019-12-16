/*Logout
	*Sends logout command to server,
	*Redirects user to login screen*/
function logout()
{
	$( '#invalid' ).addClass( 'd-none' );
	$.ajax({
		statusCode: {
			404: function(){ 
				
				window.location.replace("index.html");
			}
		},
		async: true,
		method:"POST",
		cache: false,
		url: "php/logout.php",
		data: '',
		success: function(data) { 
			window.location.replace("index.html");
			$( '#logout' ).removeClass('d-none');
		}
		}); // end ajax 

}
