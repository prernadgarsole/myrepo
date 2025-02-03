# Prompt for credentials
$username = Read-Host "Enter your Emp ID"
$password = (Read-Host "Enter your Password").Trim()
$cred = "$($username):$($password)"
$encodedCreds = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($cred))
$basicAuthValue = "Basic $encodedCreds"
$Headers = @{Authorization = $basicAuthValue}

# URLs for Reports
$priorWeekUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorWeek&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"
Invoke-WebRequest -Uri "$($priorWeekUrl)" -Method Get -Headers $Headers -OutFile tempReport.json

# CSV Output File
$UCD_Report = New-Item -ItemType File -path .\UCD_Prod_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"

# Parse JSON report
$reportJson = Get-Content 'tempReport.json' | Out-String | ConvertFrom-Json
$reportJson.items | foreach {
    $items = $_
    $items | foreach {
        $item = $_
        $env = $_.environment
        $appRequestID = $_.applicationRequestId
        $status = $_.status
        if ($status -eq "SUCCESS")
        {
            $AppRequestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/info/$appRequestID"
            Invoke-WebRequest -Uri "$($AppRequestUrl)" -Method Get -Headers $Headers -OutFile tempAppRequest.json
            $requestJson = Get-Content 'tempAppRequest.json' | Out-String | ConvertFrom-Json
            
            # Extract Application and Deployment Details
            $AppID = $requestJson.application.id
            $AppName = $requestJson.application.name
            $SnapshotID = $requestJson.snapshot.id
            $SnapshotName = $requestJson.snapshot.name
            $EnvName = $requestJson.environment.name
            $userName = $requestJson.userName

            # Convert Unix Timestamps
            $startTime = (get-date "1/1/1970").AddSeconds([int64][math]::Truncate([double]$requestJson.startTime / 1000)).ToLocalTime()
            $endTime = (get-date "1/1/1970").AddSeconds([int64][math]::Truncate([double]$requestJson.endTime / 1000)).ToLocalTime()

            $line = "$AppName,$EnvName,$userName,$startTime,$endTime"

            if (($env -like "PRD*") -or ($env -like "PROD*"))
            {
                # Fetch Component Details from Snapshot (For Production)
                $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$SnapshotID"
                Invoke-WebRequest -Uri "$($SnapshotUrl)" -Method Get -Headers $Headers -OutFile tempSnapshot.json
                $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json
                
                $snapshotJson | foreach {
                    $compName = $_.name
                    $version = $_.desiredVersions.name
                    if ($version -ne $null)
                    {
                        $line = "$line,$compName,$version"
                    }
                }
            } 
            else 
            {
                # Fetch Component Details from Manifest for Non-Production Environments
                $ManifestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/requestManifest?request=$appRequestID"
                Invoke-WebRequest -Uri "$($ManifestUrl)" -Method Get -Headers $Headers -OutFile tempManifest.json
                $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json
                
                $manifestJson.components | foreach {
                    $compName = $_.name
                    $version = $_.version.name
                    if ($version -ne $null)
                    {
                        $line = "$line,$compName,$version"
                    }
                }
            }

            Write-Output "$line"
            Add-Content $UCD_Report -Value "$line"
        }
    }
}

