
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
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version,Snapshot Details"

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

            # Attempt to fetch component details for all environments
            if ($SnapshotID -ne $null)
            {
                # Fetch deployment snapshot components for environments that use snapshots
                $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$SnapshotID"
                Invoke-WebRequest -Uri "$($SnapshotUrl)" -Method Get -Headers $Headers -OutFile tempSnapshot.json
                $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json

                $snapshotDetails = ""
                $snapshotJson | foreach {
                    $compName = $_.name
                    $version = $_.desiredVersions.name
                    if ($version -ne $null)
                    {
                        $line = $line + "," + $compName + "," + $version
                        $snapshotDetails = "$compName:$version"
                    }
                }

                # Add snapshot details column
                $line = $line + ",$snapshotDetails"
            }
            else
            {
                # For non-snapshot deployments (even if environment is production or non-prod)
                $DeploymentUrl = "https://udeploy.nnnn.net:8443/rest/deployment/manifest?application=$AppID&environment=$EnvName"
                Invoke-WebRequest -Uri "$($DeploymentUrl)" -Method Get -Headers $Headers -OutFile tempManifest.json
                $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json

                # Extract component details from the manifest (for all environments)
                $manifestDetails = ""
                $manifestJson.components | foreach {
                    $compName = $_.name
                    $version = $_.version
                    if ($version -ne $null)
                    {
                        $line = $line + "," + $compName + "," + $version
                        $manifestDetails = "$compName:$version"
                    }
                }

                # Add manifest details column
                $line = $line + ",$manifestDetails"
            }

            # Output and save the final line to CSV
            Write-Output "$line"
            Add-Content $UCD_Report -Value "$line"
        }
    }
}
