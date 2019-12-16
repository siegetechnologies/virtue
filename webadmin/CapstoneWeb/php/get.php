<?php
//Get individual objects from server
//Delete objects from server
//Import objects to server
   if($_SERVER['REQUEST_METHOD'] === 'POST')
   {
    session_start();//Get session variables
        
    $url = "https://3.92.192.42:4443/";
        //$url = "http://clayserver.myddns.me:8091/";
            $json_str = file_get_contents('php://input'); //Read post data
            $json_obj = json_decode($json_str); //decode post data
            try {
                $tokenOBJ = json_decode($_SESSION['auth_token']); //get user token
            } catch (\Throwable $th) {
                header('Content-type: text/html');
		        echo ("User Token Does Not Exist");
		        return "User Token Does Not Exist";
            }
            

        if($json_obj->Mode == 'Roles') //Craft post for single role
        {
            //get Role
            $url .= "role/get";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "roleID" => $json_obj->ID
                
            ];
        }

        if($json_obj->Mode == 'Virtue') //Craft post for single virtue
        {
            //get Virtue
            $url .= "virtue/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "virtueID" =>  $json_obj->ID,
                "mine" => false
            ];
        }

        if($json_obj->Mode == 'User') //Craft post for single user
        {
            //get User
            $url .= "user/get";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "username" => $json_obj->ID
            ];
        }

        if($json_obj->Mode == 'UserRoles') //Get roles list, for getting roles associated with users
        {
            //get User Roles
            $url .= "roles/list";
            $data = [
                "userToken" =>  $tokenOBJ->userToken
            ];
        }

        if($json_obj->Mode == 'Apps') //Craft post
        {
            //get App Details
            $url .= "application/get";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "applicationID" => $json_obj->ID
            ];
        }

        if($json_obj->Mode == 'removeVirtue')
        {
            //Destroy Virtue
            $url .= "virtue/destroy";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "virtueID" => $json_obj->ID
            ];
        }

        if($json_obj->Mode == 'removeUser')
        {
            //Remove User
            $url .= "user/remove";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "username" => $json_obj->ID
            ];
        }

        
        if($json_obj->Mode == 'removeRole')
        {
            //Remove Role
            $url .= "role/remove";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "roleID" => $json_obj->ID
            ];
        }

        if($json_obj->Mode == 'removeApp')
        {
            //Remove App
            $url .= "application/remove";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "applicationID" => $json_obj->ID
            ];
        }
        
        if($json_obj->Mode == 'importRole')
        {
            //Import Roles
            $url .= "role/import";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "roles" => $json_obj->data
            ];
        }

        if($json_obj->Mode == 'importApp')
        {
            //Import Apps
            $url .= "application/import";
            $data = [
                "userToken" =>  $tokenOBJ->userToken,
                "applications" => $json_obj->data
            ];
        }
        
        
    
        $myJSON = json_encode($data);


        $ch = curl_init($url);                                                                      
                curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");                                                                     
                curl_setopt($ch, CURLOPT_POSTFIELDS, $myJSON);                                                                  
                curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);    
                curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
                curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);                                                                     
                curl_setopt($ch, CURLOPT_HTTPHEADER, array(                                                                          
                       'Content-Type: application/json',                                                                                
                       'Content-Length: ' . strlen($myJSON))                                                                         
       );  
        curl_setopt($ch, CURLOPT_TIMEOUT, 1000);
$output = curl_exec($ch) or die("Error Contacting Server: " . curl_error($ch));   

$response = json_decode($output);


		if(isset($response->error)) {
			if(isset($_SESSION))
			{
				// remove all session variables
				session_unset(); 
	
				// destroy the session 
				session_destroy(); 
			}
			curl_close($ch);  
			$response = "error";
			header('Content-type: application/json');
			echo ($response);
			return "error";
		}

		else
		{
		//$response = "error";
		header('Content-type: text/html');
		echo ($output);
		return $output;
		}


		curl_close($ch);  
		$response = "error";
		header('Content-type: text/html');
		echo ($response);
		return false;                               
   }
?>