$UCD_Report = New-Item -ItemType File -path .\UCD_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"

$requestJson = Get-Content 'tempAppRequest_AEM-ENET_INTG2.json' | Out-String | ConvertFrom-Json

# Extract Application Details
$AppID = $requestJson.application.id
$AppName = $requestJson.application.name
$EnvName = $requestJson.environment.name
$userName = $requestJson.userName

# Convert Unix Timestamp to Local Time (Start Time)
$startTimeUnix = $requestJson.startTime
$unixTime = [int64][math]::Truncate([double]$startTimeUnix / 1000)
$tempDate = get-date "1/1/1970"
$startTime = $tempDate.AddSeconds($unixTime).ToLocalTime()

# Convert Unix Timestamp to Local Time (End Time)
$endTimeUnix = $requestJson.endTime
$unixTime = [int64][math]::Truncate([double]$endTimeUnix / 1000)
$tempDate = get-date "1/1/1970"
$endTime = $tempDate.AddSeconds($unixTime).ToLocalTime()

# Debugging: Check JSON Structure Before Extracting Component Details
Write-Output "Root Trace Children Count: $($requestJson.rootTrace.children.Count)"
Write-Output "Checking children structure..."

# Ensure correct path to extract Component Name and Version
$componentName = "N/A"
$componentVersion = "N/A"

# Traverse JSON structure safely
foreach ($child in $requestJson.rootTrace.children) {
    if ($child.type -eq "multiComponentEnvironmentIterator") {
        foreach ($subchild in $child.children) {
            if ($subchild.type -eq "componentEnvironmentIterator") {
                foreach ($subsubchild in $subchild.children) {
                    if ($subsubchild.type -eq "inventoryVersionDiff") {
                        foreach ($compProcess in $subsubchild.children) {
                            if ($compProcess.type -eq "componentProcess") {
                                $componentName = $compProcess.component.name
                                $componentVersion = $compProcess.version.name
                                Write-Output "Component Name: $componentName"
                                Write-Output "Component Version: $componentVersion"
                            }
                        }
                    }
                }
            }
        }
    }
}

# Format Data for CSV
$line = "$AppName,$EnvName,$userName,$startTime,$endTime,$componentName,$componentVersion"

# Output to Console and Append to CSV Report
Write-Output "$line"
Add-Content $UCD_Report -Value "$line"
