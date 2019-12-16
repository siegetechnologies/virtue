$RegPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Terminal Server\TSAppAllowList"
#$AppInfo = ("cmd","C:\Windows\System32\cmd.exe"), ("calc","C:\Windows\System32\calc.exe")
$AppNames = __APPNAMES
$User = __USER

Set-ItemProperty -path $RegPath -Name "fDisabledAllowList" -value 1
IF(!(Test-Path "${RegPath}\Applications"))
{
    New-Item -Path $RegPath -Name "Applications"
}

${AppInfo} | ForEach {
    ${AppName} = $_[0]
    ${AppPath} = $_[1]
    New-Item -Path "${RegPath}\Applications" -Name ${AppName}
    Set-ItemProperty -path "${RegPath}\Applications\${AppName}" -Name "Name" -value ${AppName}
    Set-ItemProperty -path "${RegPath}\Applications\${AppName}" -Name "Path" -value ${AppPath}
}

NET LOCALGROUP "Remote Desktop Users" /ADD ${User}
