

# Create a local user
$User="Virtue"
New-LocalUser ${User} -Password $(ConvertTo-SecureString "__B85PASS" -AsPlainText -Force)
NET LOCALGROUP "Remote Desktop Users" /ADD ${User}

# Not doing what you expect, __HASUSERDRIVE will be "True" or "False", meaning
# we'll get $True or $False, which are PS terms
IF( $__HASUSERDRIVE ) {
	net use \\sshfs\${User}=__USAN@10.1.1.101!2222 "__USAP"
}

# Turn on RDP and go
Set-ItemProperty -Path 'HKLM:\\System\\CurrentControlSet\\Control\\Terminal Server'-name "fDenyTSConnections" -Value 0
Enable-NetFirewallRule -DisplayGroup "Remote Desktop"

