<?php

if($_SERVER['REQUEST_METHOD'] === 'POST')
{
	$url = "https://3.92.192.42:4443/user/login";
	//$url = "http://clayserver.myddns.me:8091/user/login";

	if(isset($_POST['frm_data'])) // serialized form from JQuery
		{
			parse_str($_POST['frm_data'], $formdata); //convert string to array
	
			if(array_key_exists('user', $formdata))
			{
				$user = trim($formdata['user']); //Get user name
			}

			if(array_key_exists('pwd', $formdata))
			{
				$pwd = trim($formdata['pwd']); //Get password
			}
		}

	$data = [
		"username" =>  $user,
		"password" => $pwd
	];

	$myJSON = json_encode($data); //Conver array to Json

	//setup Curl
	$ch = curl_init($url);                                                                      
 		curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");                                                                     
		curl_setopt($ch, CURLOPT_POSTFIELDS, $myJSON);      //include JSON                                                            
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, true); 
		curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
		curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);                                                                     
		curl_setopt($ch, CURLOPT_HTTPHEADER, array(                                                                          
   			'Content-Type: application/json',                                                                                
   			'Content-Length: ' . strlen($myJSON))                                                                       
		);  
	curl_setopt($ch, CURLOPT_TIMEOUT, 1000);
$output = curl_exec($ch) or die("Error Contacting Server: " . curl_error($ch));   //Send Curl

$response = json_decode($output); //Decode JSON response


		if(isset($response->error)) { //Kill everything on error
			if(isset($_SESSION))
			{
				// remove all session variables
				session_unset(); 
	
				// destroy the session 
				session_destroy(); 
			}
			curl_close($ch);  //close Curl
			$response = "error";
			header('Content-type: application/json');
			echo ($response); //Send response to client
			return "error";
		}

		else if(isset($response->userToken)) //Valid credentials
		//else
		{
		$session_user = "user_name";
		$session_name = "auth_token";

		//session_save_path('cgi-bin\tmp');
		session_start();
		
		$_SESSION[$session_user] = $data["username"]; //setup usertoken Session variables
		$_SESSION[$session_name] = $output;
		session_write_close();

		$response = "success"; //Send response to client
		//$response = "error";
		header('Content-type: text/html');
		echo ($response);
		return true;
		}


		curl_close($ch);  
		$response = "error";
		header('Content-type: text/html');
		echo ($response);
		return false;                                 

	/* $url = "https://184.72.152.190:4443/user/login";*/
	
}


?>