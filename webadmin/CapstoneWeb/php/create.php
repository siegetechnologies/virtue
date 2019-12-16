<?php
   if($_SERVER['REQUEST_METHOD'] === 'POST')
   {
    session_start();
    $url = "https://3.92.192.42:4443/";
       // $url = "http://clayserver.myddns.me:8091/";
        $json = file_get_contents('php://input');
        $obj = json_decode($json, true);

        
        try {
            $token = json_decode($_SESSION['auth_token'], true); //get user token
        } catch(Exception $e) {
            header('Content-type: text/html');
            echo ("User Token Does Not Exist");
            return "User Token Does Not Exist";
         }

        if(array_key_exists('userToken', $token))
        {
            //get User
            $token = $token["userToken"];
        }

        if (array_key_exists("Function", $obj))
        {
             //  user role authorize
             if($obj['Function'] == "Authorize")
             {
                $url .= "user/role/authorize";
             }
             else
             {
                $url .= "user/role/unauthorize";
             }
           
            $data = [
                'userToken' => $token, 
                'username' => $obj['username'], 
                'roleID' => $obj['roleID']
            ];
        } 

        else if(array_key_exists('username', $obj))
        {
            // if(array_key_exists('roleID'), $obj)
            //     {
            //           //user role authorize
            // $url .= "user/role/authorize";
            // $data = [
            //     'userToken' => $token, 
            //     'username' => $obj['username'], 
            //     'roleID' => $obj['roleID']
            // ];
            //     }
            //     else{
                      //get User
            $url .= "user/create";
            $data = [
                'userToken' => $token, 
                'username' => $obj['username'], 
                'password' => $obj['password'], 
                'isAdmin' => $obj['isAdmin']
            ];    
            //\    }
          
        }
        else if(array_key_exists('roleID', $obj))
        {
            //get Role
            $url .= "role/create";
            $data = [
                'userToken' => $token, 
                'roleID' => $obj['roleID'], 
                'mounts' => $obj['mounts'],  
                'isVirtlet' => $obj['isVirtlet'],  
                'ramsize' => $obj['ramsize'],  
                'cpucount' => $obj['cpucount'],  
                'applicationIDs' => $obj['applicationIDs'],   
                'sensors' => $obj['sensors'],  
                'filtering' => $obj['filtering']];
        }
        else if(array_key_exists('applicationID', $obj))
        {
            //get App
            $url .= "application/create";
            if($obj['desktop'] == "")
            {
                $data = [
                    'userToken' => $token, 
                    'applicationID' => $obj['applicationID'], 
                    'type' => $obj['type'], 
                    'install' => $obj['install'], 
                    'postInstall' => $obj['postInstall'], 
                    'launchCmd' => $obj['launchCmd'], 
                    'icon' => $obj['icon'], 
                ];
            }
            else
            {
                $data = [
                    'userToken' => $token, 
                    'applicationID' => $obj['applicationID'], 
                    'type' => $obj['type'], 
                    'install' => $obj['install'], 
                    'postInstall' => $obj['postInstall'], 
                    'launchCmd' => $obj['launchCmd'], 
                    'icon' => $obj['icon'], 
                    'desktop' => $obj['desktop']
                ];
            }
           
        }

        
/* 
        else if(array_key_exists('roleID', $obj))
        {
            //get Role
            $url .= "user/role/authorize";
            $data = [
                'userToken' => $token, 
                'username' => $obj['username'], 
                'roleID' => $obj['roleID'],  
        } */
        $newJson = json_encode($data);

        $ch = curl_init($url);                                                                      
                curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "POST");                                                                     
                curl_setopt($ch, CURLOPT_POSTFIELDS, $newJson);                                                                  
                curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);   
                curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
                curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);                                                                      
                curl_setopt($ch, CURLOPT_HTTPHEADER, array(                                                                          
                        'Content-Type: application/json',                                                                                
                        'Content-Length: ' . strlen($newJson))                                                                       
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

   
        // $url = "https://184.72.152.190:4443/"
        // if(isset($_POST['mode']) == 'Virtue')
        // {
        //     //get Virtue
        //     $url .= "virtue/list";
        // }
        // else if(isset($_POST['mode']) == 'User')
        // {
        //     //get User
        //     $url .= "user/list";
        // }
        // else if(isset($_POST['mode']) == 'Role')
        // {
        //     //get Role
        //     $url .= "role/list";
        // }
        // else if(isset($_POST['mode']) == 'App')
        // {
        //     //get App
        //     $url .= "application/list";
        // }
        // else
        // {
        //     // get Virtue
        //     $url .= "virtue.json";
        // }

?>
