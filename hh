# prompt for credential for testing
# the password is not masked
# For implementation we will use another method for providing credentials 
$username = Read-Host "Enter your Emp ID"
$password = (Read-Host "Enter your Password").Trim()

$cred = "$($username):$($password)"

$priorWeekUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorWeek&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"

$priorMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=priorMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"

$currentMonthUrl = "https://udeploy.nnnn.net:8443/rest/report/adHoc?dateRange=currentMonth&orderField=application&sortType=asc&type=com.urbancode.ds.subsys.report.domain.deployment_report.DeploymentReport"

$encodedCreds = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($cred))
$basicAuthValue = "Basic $encodedCreds"
$Headers = @{Authorization = $basicAuthValue}

Invoke-WebRequest -Uri "$($priorWeekUrl)" -Method Get -Headers $Headers -OutFile tempReport.json
$UCD_Report = New-Item -ItemType File -path .\UCD_Prod_Report.csv -Force
Add-Content $UCD_Report -Value "Application Name,Environment Name,User Name,Start Time,End Time,Component Name,Component Version"
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
            # Fetch the application request details
            $AppRequestUrl = "https://udeploy.nnnn.net:8443/cli/applicationProcessRequest/info/$appRequestID"
            Invoke-WebRequest -Uri "$($AppRequestUrl)" -Method Get -Headers $Headers -OutFile tempAppRequest.json
            $requestJson = Get-Content 'tempAppRequest.json' | Out-String | ConvertFrom-Json
            $AppID = $requestJson.application.id
            $AppName = $requestJson.application.name
            $SnapshotID = $requestJson.snapshot.id
            $SnapshotName = $requestJson.snapshot.name
            $EnvName = $requestJson.environment.name
            $userName = $requestJson.userName
            $startTimeUnix = $requestJson.startTime
            $unixTime = [int64][math]::Truncate([double]$startTimeUnix / 1000)
            $tempDate = get-date "1/1/1970"
            $startTime = $tempDate.AddSeconds($unixTime).ToLocalTime()
            $endTimeUnix = $requestJson.endTime
            $unixTime = [int64][math]::Truncate([double]$endTimeUnix / 1000)
            $tempDate = get-date "1/1/1970"
            $endTime = $tempDate.AddSeconds($unixTime).ToLocalTime()
            $line = $AppName
            $line = $line + "," + $EnvName
            $line = $line + "," + $userName
            $line = $line + "," + $startTime
            $line = $line + "," + $endTime
            
            # Logic for Production environments (Snapshot-based deployments)
            if (($env -like "PRD*") -or ($env -like "PROD*"))
            {
                $SnapshotUrl = "https://udeploy.nnnn.net:8443/cli/snapshot/getSnapshotVersions?application=$AppID&snapshot=$SnapshotID"
                Invoke-WebRequest -Uri "$($SnapshotUrl)" -Method Get -Headers $Headers -OutFile tempSnapshot.json
                $snapshotJson = Get-Content 'tempSnapshot.json' | Out-String | ConvertFrom-Json
                $snapshotJson | foreach {
                    $compName=$_.name
                    $version=$_.desiredVersions.name
                    if ($version -ne $null)
                    {
                        $line = $line + "," + $compName
                        $line = $line + "," + $version
                    }
                }
            } 
            else {
                # Logic for non-production environments that do not use Snapshots
                $nonProdEnvList = @("UNIT", "UNIT2", "INTG", "INTG2", "PERF", "PRE-PROD", "PRDD", "PRDW")
                if ($nonProdEnvList -contains $env)
                {
                    # URL for retrieving the manifest for non-production deployments
                    $ManifestUrl = "https://udeploy.nnnn.net:8443/rest/deployment/manifest?application=$AppID&environment=$EnvName"
                    Invoke-WebRequest -Uri "$($ManifestUrl)" -Method Get -Headers $Headers -OutFile tempManifest.json
                    $manifestJson = Get-Content 'tempManifest.json' | Out-String | ConvertFrom-Json
                    $manifestJson.components | foreach {
                        $compName = $_.name
                        $version = $_.version
                        if ($version -ne $null)
                        {
                            $line = $line + "," + $compName
                            $line = $line + "," + $version
                        }
                    }
                }
            }

            # Write line to CSV
            Write-Output "$line"
            Add-Content $UCD_Report -Value "$line"
        }
    }
}
