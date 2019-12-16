# Overview

This folder contains all devops utilities used by MAVEN. This includes vagrant
files used to stand up both build and test machines. These machines can be
created either in a local virtualbox backend or on EC2.  The `ansible` directory
contains playbook files to bootstrap and setup both build and test servers.

## Vagrant notes
Vagrant is setup to create any number of machines in either virtualbox or aws.  These
machines are defined in the config.yml file in the vagrant/buildserver directory.  In
this file, the machine sources are defined as well as any additional information needed
that is specific to the provider.  No AWS credentials are stored in the repo.  Instead, there
is an aws.yml.skeleton file that provides details that need to be filled in locally.  Simply
copy that file to `aws.yml` and fill in the necessary details and vagrant should be able to
provision machines on EC2.

## Ansible notes
Once vagrant has created the virtual machine, Ansible is used to do provisioning and setup. 
All it requires on a target machine is SSH access.  It will install all necessary packages, 
setup the build environment and pull down a copy of the MAVEN repository for build.  Access
do EC2 is done via the ec2 python interface, and so credentials need to be stored in the `~/.aws`
directory according to online docs.
    

	TODO: Check if this is the right document.
	https://boto3.readthedocs.io/en/latest/guide/quickstart.html
    
In order to provide access to the git private repo, an OAuth access token for your user is
required.  In order to get one of these, follow the directions **HERE** (*TODO::Get this reference*)
Once you have the token it needs to be stored in an encrypted vault file.  This file contains all variables
used by ansible that are private and should not be shared.  To create this vault, run the following from the
root of the repo:

    ansible-vault create devops/ansible/buildserver/group_vars/all/vault

(note, ansible-vault will error with File Not Found if the $EDITOR env variable is set to something that is not installed, eg VIM by default)
    
and set the password on the file.  This file is gitignored, so there should be no worry about checking it
in to the repository.  The vault-plain file gives a sample of what should be in the file.  Once it is created
you can run

    ansible-vault edit devops/ansible/buildserver/group_vars/all/vault
    
to edit the file and set the vault_git_oauth_token variable.
