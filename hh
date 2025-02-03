
# Prompt for credentials (not masked)
$username = Read-Host "Enter your Emp ID"
$password = (Read-Host "Enter your Password").Trim()

$cred = "$($username):$($password)"

# URLs for prior week, prior month, and current month reports
$priorWeekUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorWeek&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"
$priorMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"
$currentMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=currentMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"

# Encode credentials in base64 for basic authentication
$encodedCreds = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($cred))
$basicAuthValue = "Basic $encodedCreds"
$Headers = @{Authorization = $basicAuthValue}

# Fetch the report for prior week
Invoke-WebRequest -Uri "$($priorWeekUrl)" -Method Get -Headers $Headers -OutFile tempReport.json

# Create the output CSV file
$UCD_Report = New-Item -ItemType File -Path .\UCD_Prod_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"

# Read and parse the report JSON
$reportJson = Get-Content 'tempReport.json' | Out-String | ConvertFrom-Json

# Process each deployment item
$reportJson.items | foreach {
    $items = $_
    $items | foreach {
        $item = $_
        $env = $_.environment
        $appRequestID = $_.applicationRequestId
        $status = $_.status
        if ($status -eq "SUCCESS")
        {
            # Fetch application request details
            $AppRequestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/info/$appRequestID"
            Invoke-WebRequest -Uri "$($AppRequestUrl)" -Method Get -Headers $Headers -OutFile tempAppRequest.json
            $requestJson = Get-Content 'tempAppRequest.json' | Out-String | ConvertFrom-Json

            # Extract application details
            $AppID = $requestJson.application.id
            $AppName = $requestJson.application.name
            $SnapshotID = $requestJson.snapshot.id
            $SnapshotName = $requestJson.snapshot.name
            $EnvName = $requestJson.environment.name
            $userName = $requestJson.userName

            # Convert start and end times from Unix timestamp to readable date
            $startTimeUnix = $requestJson.startTime
            $unixTime = [int64][math]::Truncate([double]$startTimeUnix / 1000)
            $tempDate = get-date "1/1/1970"
            $startTime = $tempDate.AddSeconds($unixTime).ToLocalTime()

            $endTimeUnix = $requestJson.endTime
            $unixTime = [int64][math]::Truncate([double]$endTimeUnix / 1000)
            $endTime = $tempDate.AddSeconds($unixTime).ToLocalTime()

            # Initialize the output line with basic app and environment info
            $line = "$AppName,$EnvName,$userName,$startTime,$endTime"

            # Check if the environment is a non-production type (like UNIT, INTG, etc.)
            $nonProdEnvs = @("UNIT", "UNIT2", "INTG", "INTG2", "PERF", "PRE-PROD", "PRDD", "PRDW")
            if ($nonProdEnvs -contains $EnvName)
            {
                # Fetch deployment manifest for non-production environments
                $DeploymentUrl = "https://udeploy.nnnn.net:8443/rest/deployment/manifest?application=$AppID&environment=$EnvName"
                Invoke-WebRequest -Uri "$($DeploymentUrl)" -Method Get -Headers $Headers -OutFile tempManifest.json
                $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json

                # Extract component details from the manifest (similar to snapshots)
                $manifestJson.components | foreach {
                    $compName = $_.name
                    $version = $_.version
                    if ($version -ne $null)
                    {
                        $line = $line + "," + $compName + "," + $version
                    }
                }
            }
            else
            {
                # For production environments, use snapshots to fetch component versions
                $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$SnapshotID"
                Invoke-WebRequest -Uri "$($SnapshotUrl)" -Method Get -Headers $Headers -OutFile tempSnapshot.json
                $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json
                $snapshotJson | foreach {
                    $compName = $_.name
                    $version = $_.desiredVersions.name
                    if ($version -ne $null)
                    {
                        $line = $line + "," + $compName + "," + $version
                    }
                }
            }

            # Output and save the final line to CSV
            Write-Output "$line"
            Add-Content $UCD_Report -Value "$line"
        }
    }
}
