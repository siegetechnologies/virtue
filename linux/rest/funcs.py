#!/usr/bin/python3



import funcs_application
import funcs_role
import funcs_user
import funcs_virtue
import funcs_resource
#import funcs_system
#import funcs_test
import funcs_usertoken
import funcs_sensors


#better way to do this...


funcs = {
	"application":{
		"get": funcs_application.application_get_func,
		"list": funcs_application.application_list_func,
		"create": funcs_application.application_create_func,
		"import": funcs_application.application_import_func,
		"remove": funcs_application.application_remove_func,
	},
	"role": {
		"get": funcs_role.role_get_func,
		"create": funcs_role.role_create_func,
		"remove": funcs_role.role_remove_func,
		"list": funcs_role.role_list_func,
		"import": funcs_role.role_import_func,
	},
	"user": {
		"login": funcs_user.user_login_func,
		"logout": funcs_user.user_logout_func,
		"password": funcs_user.user_password_func,
		"create": funcs_user.user_create_func,
		"roleauthorize": funcs_user.user_role_authorize_func,
		#this may change to be /user/roleauthorize or something
		"role": {
			"list": funcs_user.user_role_list_func,
			"authorize": funcs_user.user_role_authorize_func,
			"unauthorize": funcs_user.user_role_unauthorize_func,
		},
		"key": funcs_user.user_key_func,
		"list": funcs_user.user_list_func,
		"get":funcs_user.user_get_func,
		"remove":funcs_user.user_remove_func,
	},
	"virtue": {
		"cleanup": funcs_virtue.virtue_cleanup_func,
		"list": funcs_virtue.virtue_list_func,
		"create": funcs_virtue.virtue_create_func,
		"createall": funcs_virtue.virtue_create_all_func,
		"destroy": funcs_virtue.virtue_destroy_func,
		"destroyall": funcs_virtue.virtue_destroy_all_func,
	},
	"resource": {
		"get": funcs_resource.resource_get_func,
		"list":funcs_resource.resource_list_func,
		"attach":funcs_resource.resource_attach_func,
		"detatch":funcs_resource.resource_detach_func,
	},
#	"system": {
#		"export": funcs_system.system_export_func,
#		"import":funcs_system.system_import_func,
#	},
#	"test": {
#		"import": {
#			"user": funcs_test.test_import_user_func,
#			"application": funcs_test.test_import_application_func,
#			"role": funcs_test.test_import_role_func,
#		},
#	},
	"usertoken": {
		"list": funcs_usertoken.usertoken_list_func,
	},
	"sensors": {
		"list": funcs_sensors.sensors_list,
		"create": funcs_sensors.sensors_create,
	}
}

from inspect import signature
def parse(name, args, ip):
	a = name
	f = funcs
	while isinstance(f, dict) and a and a[0] in f:
		f = f[a[0]]
		a = a[1:]


	if callable(f):
		sig = signature(f)
		if('ip') in sig.parameters:
			return f(args, ip=ip)
		return f(args)
	else:
		print("error " + str(name))
