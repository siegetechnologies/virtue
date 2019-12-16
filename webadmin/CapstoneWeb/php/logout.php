<?php
//Logs user out of page
//Asks server to delete user token
//Delete session variable
if($_SERVER['REQUEST_METHOD'] === 'POST')
{
	$url = "https://3.92.192.42:4443/user/logout";
    //$url = "http://clayserver.myddns.me:8091/user/logout";
    session_start(); //Gain access to user token

	//Setup curl
	$ch = curl_init($url);                                                                      
 		curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");                                                                     
		curl_setopt($ch, CURLOPT_POSTFIELDS, $_SESSION['auth_token']);   //include user token                                                                
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);  
		curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
		curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);                                                                      
		curl_setopt($ch, CURLOPT_HTTPHEADER, array(                                                                          
   			'Content-Type: application/json',                                                                                
   			'Content-Length: ' . strlen($_SESSION['auth_token']))                                                                       
		);  
    curl_setopt($ch, CURLOPT_TIMEOUT, 1000); 
    
   


$output = curl_exec($ch) or die("Error Contacting Server: " . curl_error($ch)); //Send Curl request


if(isset($_SESSION)) //if session exists
{
    // remove all session variables
    session_unset(); 

    // destroy the session 
    session_destroy(); 
}
}
?>