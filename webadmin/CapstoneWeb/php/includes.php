<?php
//Get list data from server
//Mode used as switch between the different objects
   if($_SERVER['REQUEST_METHOD'] === 'POST')
   {
    session_start();

     $url = "https://3.92.192.42:4443/";
       // $url = "http://clayserver.myddns.me:8091/";

       try {
            $tokenOBJ = json_decode($_SESSION['auth_token']); //get user token
        } catch(Exception $e) {
            header('Content-type: text/html');
            echo ("User Token Does Not Exist");
            return "User Token Does Not Exist";
         }

        parse_str($_POST['mode'], $mode); //Parse response
        if(array_key_exists('Virtue', $mode)) //Craft Virtue post
        {
            //get Virtue
            $url .= "virtue/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "mine" => false
            ];
        }
        else if(array_key_exists('User', $mode)) //Craft User post
        {
            //get User
            $url .= "user/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken
            ];
        }
        else if(array_key_exists('Role', $mode)) //Craft Role post
        {
            //get Role
            $url .= "role/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken
            ];
        }
        else if(array_key_exists('App', $mode)) //Craft App post
        {
            //get App
            $url .= "application/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken
            ];
        }
 

    
        $myJSON = json_encode($data); //Craft Json from data object and usertoken

        $ch = curl_init($url); //Curl setup                                                                     
        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");                                                                     
        curl_setopt($ch, CURLOPT_POSTFIELDS, $myJSON);         //Include Json object                                                         
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);     
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
		curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);                                                                    
        curl_setopt($ch, CURLOPT_HTTPHEADER, array(                                                                          
               'Content-Type: application/json',                                                                                
               'Content-Length: ' . strlen($myJSON))                                                                         
);  
        curl_setopt($ch, CURLOPT_TIMEOUT, 1000);
$output = curl_exec($ch) or die("Error Contacting Server: " . curl_error($ch));   //Send Curl

$response = json_decode($output);//Parse JSON response


		if(isset($response->error)) { //Error, destroy session variables
			if(isset($_SESSION))
			{
				// remove all session variables
				session_unset(); 
	
				// destroy the session 
				session_destroy(); 
			}
			curl_close($ch);  //close curl
			$response = "error";
			header('Content-type: application/json');
			echo ($response); //send response to client
			return "error";
		}

		else
		{
		//$response = "error";
		header('Content-type: text/html');
		echo ($output); //send response to client
		return $output;
		}


		curl_close($ch);  
		$response = "error";
		header('Content-type: text/html');
		echo ($response);
		return false;                               
   }
?>