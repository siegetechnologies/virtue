#!/usr/bin/python3






import funcs_application



#better way to do this...


application_func_throughs = {"get": funcs_application.application_get_func}

def application_func(args):
	if(args[0]) in application_func_throughs:
		return application_func_throughs[args[0]](args[1:])



role_func_throughs = {"get": funcs_role.role_get_func, "create":funcs_role.role_create_func, "list":funcs_role.role_list_func}

def role_func(args):
	if(args[0]) in role_func_throughs:
		return role_func_throughs[args[0]](args[1:])



user_func_throughs = {"login": funcs_user.user_login_func, "logout": funcs_user.user_logout_func, "role": funcs_login.user_login_func, "virtue": funcs_user.user_virtue_func, "list": funcs_user.user_list_func, "get":funcs_user.user_get_func, "role":funcs_user.user_role_func}

def user_func(args):
	if(args[0]) in user_func_throughs:
		return user_func_throughs[args[0]](args[1:])



virtue_func_throughs = {"get": funcs_virtue.virtue_get_func, "create": funcs_virtue.virtue_create_func, "launch": funcs_virtue.virtue_launch_func, "stop": funcs_virtue.virtue_stop_func, "destroy": funcs_virtue.virtue_destroy_func, "application":funcs_virtue.virtue_application_func}

def virtue_func(args):
	if(args[0]) in virtue_func_throughs:
		return virtue_func_throughs[args[0]](args[1:])


application_func_throughs = {"list": funcs_application_application_list_func}

def application_func(args):
	if(args[0]) in application_func_throughs:
		return application_func_throughs[args[0]](args[1:])

resource_func_throughs = {"get": funcs_resource.resource_get_func, "list":funcs_resource.resource_list_func, "attach":funcs_resource.resource_attach_func, "detatch":funcs_resource.resource_detach_func}

def resource_func(args):
	if(args[0]) in resource_func_throughs:
		return resource_func_throughs[args[0]](args[1:])


system_func_throughs = {"export": funcs_system.system_export_func, "import":funcs_system.system_import_func}

def system_func(args):
	if(args[0]) in system_func_throughs:
		return system_func_throughs[args[0]](args[1:])


test_func_throughs = {"export": funcs_test.test_export_func}

def test_func(args):
	if(args[0]) in test_func_throughs:
		return test_func_throughs[args[0]](args[1:])


usertoken_func_throughs = {"list": funcs_usertoken.usertoken_list_func}

def usertoken_func(args):
	if(args[0]) in usertoken_func_throughs:
		return usertoken_func_throughs[args[0]](args[1:])

