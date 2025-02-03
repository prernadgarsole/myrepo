# Define CSV file path
$UCD_Report = ".\UCD_Report.csv"

# Create or overwrite CSV file with headers
"Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version" | Set-Content $UCD_Report

# Read JSON from file
$requestJson = Get-Content 'tempAppRequest_AEM-ENET_INTG2.json' | Out-String | ConvertFrom-Json

# Extract Application Details
$AppID = $requestJson.application.id
$AppName = $requestJson.application.name
$EnvName = $requestJson.environment.name
$userName = $requestJson.userName

# Convert Unix Timestamp to Local Time (Start Time)
$startTimeUnix = $requestJson.startTime
$startTime = (Get-Date "1970-01-01").AddSeconds($startTimeUnix / 1000).ToLocalTime()

# Convert Unix Timestamp to Local Time (End Time)
$endTimeUnix = $requestJson.endTime
$endTime = (Get-Date "1970-01-01").AddSeconds($endTimeUnix / 1000).ToLocalTime()

# Initialize Component Details
$componentName = "N/A"
$componentVersion = "N/A"

# Traverse JSON structure to find component name and version
foreach ($child in $requestJson.rootTrace.children) {
    if ($child.type -eq "multiComponentEnvironmentIterator") {
        foreach ($subchild in $child.children) {
            if ($subchild.type -eq "componentEnvironmentIterator") {
                foreach ($subsubchild in $subchild.children) {
                    if ($subsubchild.type -eq "inventoryVersionDiff") {
                        foreach ($compProcess in $subsubchild.children) {
                            if ($compProcess.type -eq "componentProcess") {
                                # Extract Component Name & Version
                                $componentName = $compProcess.component.name
                                $componentVersion = $compProcess.version.name

                                # Format Data for CSV
                                $csvLine = "$AppName,$EnvName,$userName,$startTime,$endTime,$componentName,$componentVersion"

                                # Append to CSV File
                                Add-Content $UCD_Report -Value $csvLine
                            }
                        }
                    }
                }
            }
        }
    }
}

Write-Output "Report generated successfully at $UCD_Report"
