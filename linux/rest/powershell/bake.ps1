#$AppInfo = "office365proplus", "firefox"
$AppInfo = __CHOCO_TARGETS
#$AppPaths = "C:\Windows\System32\cmd.exe","C:\Windows\System32\calc.exe"
$AppPaths = __APPS
$RegPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Terminal Server\TSAppAllowList"

# Installing chocolatey
Set-ExecutionPolicy Bypass -Scope Process -Force; iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

# We may not need this, install it anyway
choco install -y sshfs

${AppInfo} | ForEach {
    choco install $_  -y --force
}

# Setting up remoteapp for the given applications
Set-ItemProperty -path $RegPath -Name "fDisabledAllowList" -value 1
IF(!(Test-Path "${RegPath}\Applications"))
{
    New-Item -Path $RegPath -Name "Applications"
}


${AppPaths} | ForEach {
	${AppPath} = $_
	# Split-Path -LeafBase isn't in our version of powershell, do hacky stuff instead
    ${AppName} = $(Split-Path -Leaf ${AppPath}).split(".")[0]
    New-Item -Path "${RegPath}\Applications" -Name ${AppName}
    Set-ItemProperty -path "${RegPath}\Applications\${AppName}" -Name "Name" -value ${AppName}
    Set-ItemProperty -path "${RegPath}\Applications\${AppName}" -Name "Path" -value ${AppPath}
}

# check if there is another drive
# may not be present if we don't care about persistance
IF( (Get-Disk).length -ge 2 )
{
	# Unformatted? Would happen the first time
	IF( (Get-Disk -Number 1).PartitionStyle  -eq "RAW" ) {
		Write-Host( "Partitioning RAW disk" )
		Initialize-Disk -Number 1 -PartitionStyle MBR
		New-Partition -DiskNumber 1 -DriveLetter D -UseMaximumSize
		Format-volume -DriveLetter D
	} ELSE {
		Write-Host( "Attaching already partitioned disk" )
		# Disk is already partitioned
		# Hopefully it will be brought online as D:\
		Set-Disk -Number 1 -IsOffline $False
	}
	Set-Disk -Number 1 -IsReadOnly $False
	Set-ItemProperty -path  "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" -Name "ProfilesDirectory" -value "D:\Users"
}
