#requires -version 5
<#
	.SYNOPSIS
		Sets up the project's .asset folder: either by creating the shared
		asset library on Google Drive, or by taking in a config.json that
		was handed out by whoever created it.

	.DESCRIPTION
		The library itself is created once for the whole team. Creating it
		signs you in, makes the library folder, the blobs and Assets
		folders inside it and the two documents that act as the catalog and
		the lock table, shares them with the members you name, and writes
		the configuration the Editor reads.

		Every other member only needs a copy of that config.json; this
		script puts it in place for them, and the Editor signs them in by
		itself on first start.

	.EXAMPLE
		.\Startup.ps1
		.\Startup.ps1 -Mode Member -Config "C:\Users\me\Downloads\config.json"
		.\Startup.ps1 -Mode Owner -ClientId "..." -ClientSecret "..." -Members "a@example.com","b@example.com"
#>
param(
	[ValidateSet('', 'Owner', 'Member')][string]$Mode = '',
	[string]$Config = '',
	[string]$ClientId = '',
	[string]$ClientSecret = '',
	[string[]]$Members = @(),
	[string]$LibraryName = 'SeedCore Shared Library',
	[string]$Workspace = 'UserProject',
	[string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'

# $PSScriptRoot はパラメータの既定値を評価する時点ではまだ空なので、ここで解決する。
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ProjectRoot)
{
	$ProjectRoot = (Resolve-Path (Join-Path $scriptDirectory '..\..')).Path
}
Write-Host "プロジェクト: $ProjectRoot"

# --- Python -------------------------------------------------------------
# コード生成は MSBuild のビルド前イベントで走るので、py が PATH に無いと
# ビルドそのものが通らない。ここで入っているか確かめる。
Write-Host ''
if (Get-Command py -ErrorAction SilentlyContinue)
{
	Write-Host "Python は既にインストールされています。"
	& py --version
}
else
{
	$answer = ''
	while ($answer -ne 'Y' -and $answer -ne 'N')
	{
		$answer = (Read-Host 'Python が検出されませんでした。インストールを開始しますか？ (Y/N)').Trim().ToUpper()
	}
	if ($answer -eq 'N')
	{
		return
	}

	# インストーラは一時フォルダへ落とす。プロジェクトの中に残らないようにするため。
	$installer = Join-Path $env:TEMP 'python_installer.exe'
	Write-Host 'インストーラーをダウンロード中...'
	Invoke-WebRequest -Uri 'https://www.python.org/ftp/python/3.14.4/python-3.14.4-amd64.exe' -OutFile $installer

	Write-Host 'インストールを実行中...'
	Start-Process -FilePath $installer -ArgumentList '/quiet', 'InstallAllUsers=1', 'PrependPath=1', 'Include_pip=1' -Wait
	Remove-Item $installer -Force -ErrorAction SilentlyContinue
	Write-Host 'インストールが完了しました。'

	# PrependPath で PATH は通るが、それが効くのは次に開いたウィンドウから。
	Write-Host '  (py を使うには、このウィンドウを閉じて開き直してください)'
}

# --- shared assets ------------------------------------------------------
# .asset は隠しフォルダなので、作成と属性付けはどちらの道でも通る形でここに置く。
# 先頭のドットだけでは Windows で隠れないため、属性でも隠しておく。
$directory = Join-Path $ProjectRoot '.asset'
if (-not (Test-Path $directory))
{
	New-Item -ItemType Directory -Path $directory | Out-Null
}
$folder = Get-Item $directory -Force
$folder.Attributes = $folder.Attributes -bor [IO.FileAttributes]::Hidden
$path = Join-Path $directory 'config.json'

# 設定が既にあるなら、このスクリプトのやることは終わっている。
if ((Test-Path $path) -and -not $Mode)
{
	Write-Host ''
	Write-Host "共有アセットの設定は既にあります: $path"
	return
}

# ライブラリを作るのはチームで1人だけで、残りのメンバーはその設定を置くだけ。
# やることが正反対なので、最初にどちらなのかを聞く。
if (-not $Mode)
{
	Write-Host ''
	Write-Host '共有アセットの設定をします。どちらですか？'
	Write-Host '  1. 共有ライブラリを作る (チームで1人だけ。まだ誰も作っていない場合)'
	Write-Host '  2. 配られた config.json を置く (それ以外の全員)'
	while (-not $Mode)
	{
		$answer = (Read-Host '  1 か 2').Trim()
		if ($answer -eq '1')
		{
			$Mode = 'Owner'
		}
		elseif ($answer -eq '2')
		{
			$Mode = 'Member'
		}
	}
}

# メンバー側は Google に触らない。もらったファイルを .asset へ写すだけで、
# ログインは Editor が初回起動時に各自のアカウントで済ませる。
if ($Mode -eq 'Member')
{
	while (-not $Config)
	{
		Write-Host ''
		Write-Host '配られた config.json をこのウィンドウへドラッグして Enter を押してください。'
		Write-Host '(まだ持っていない場合は、何も入れずに Enter)'
		$Config = (Read-Host '  config.json').Trim().Trim('"')
		if (-not $Config)
		{
			Write-Host ''
			Write-Host "受け取ったら、このフォルダへ置いてください: $path"
			Write-Host '  (作った人に聞いてください。リポジトリには入っていません)'
			return
		}
		if (-not (Test-Path $Config))
		{
			Write-Host "  見つかりません: $Config"
			$Config = ''
		}
	}

	# 中身の確認まではしておく。別のファイルを掴んでいると Editor 側で分かりにくく失敗する。
	$given = Get-Content -Raw -Encoding UTF8 $Config | ConvertFrom-Json
	foreach ($required in @('clientId', 'clientSecret', 'catalogDocumentId', 'lockDocumentId', 'blobFolderId'))
	{
		if (-not $given.$required)
		{
			throw "$Config は共有アセットの設定ではないようです ($required がありません)。"
		}
	}

	Copy-Item -Path $Config -Destination $path -Force
	Write-Host ''
	Write-Host "設定を置きました: $path"
	Write-Host ''
	Write-Host '次にやること:'
	Write-Host '  1. Editor を起動すると、初回だけブラウザで Google ログインを求められます'
	Write-Host '  2. 共有ライブラリを作った人から、Drive の共有が届いているか確認してください'
	return
}

# Cloud Console から落とした OAuth クライアントの JSON があれば、ID とシークレットを
# 手で渡さなくて済む。スクリプトの隣と、共有ツールの置き場の両方を見る。
if (-not $ClientId -or -not $ClientSecret)
{
	$secretFile = $null
	foreach ($directory in @($scriptDirectory, (Join-Path $ProjectRoot 'Tools\AssetSharing')))
	{
		if (-not (Test-Path $directory))
		{
			continue
		}
		$secretFile = Get-ChildItem -Path $directory -File | Where-Object { $_.Name -like '*ClientSecret*.json' -or $_.Name -like 'client_secret*.json' } | Select-Object -First 1
		if ($secretFile)
		{
			break
		}
	}
	if (-not $secretFile)
	{
		throw "OAuth クライアントの JSON が見つかりません。Cloud Console から落として $scriptDirectory に置くか、-ClientId と -ClientSecret を渡してください。"
	}

	# 種類によって installed か web のどちらかに入っているので、両方見る。
	$secretJson = Get-Content -Raw -Encoding UTF8 $secretFile.FullName | ConvertFrom-Json
	$client = if ($secretJson.installed) { $secretJson.installed } else { $secretJson.web }
	if (-not $client)
	{
		throw "$($secretFile.Name) の形が想定と違います (installed も web もありません)。"
	}
	if (-not $ClientId)
	{
		$ClientId = $client.client_id
	}
	if (-not $ClientSecret)
	{
		$ClientSecret = $client.client_secret
	}
	Write-Host "OAuth クライアント: $($secretFile.Name)"
}

# 共有相手を引数で並べるのは面倒なので、渡されなければその場で聞く。
if ($Members.Count -eq 0)
{
	Write-Host ''
	Write-Host '共有するメンバーのメールアドレスを1行ずつ入力してください (何も入れずに Enter で終了)'
	$entered = New-Object System.Collections.ArrayList
	while ($true)
	{
		$line = (Read-Host ("  メンバー {0}" -f ($entered.Count + 1))).Trim()
		if (-not $line)
		{
			break
		}
		[void]$entered.Add($line)
	}
	$Members = @($entered)
}
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$drive = 'https://www.googleapis.com/drive/v3/files'
$docs = 'https://docs.googleapis.com/v1/documents'

# --- sign in ------------------------------------------------------------
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
$redirect = "http://127.0.0.1:$($listener.LocalEndpoint.Port)/"
$scope = [Uri]::EscapeDataString('openid email https://www.googleapis.com/auth/drive https://www.googleapis.com/auth/documents')
Write-Host 'ブラウザでログインしてください...'
Start-Process "https://accounts.google.com/o/oauth2/v2/auth?client_id=$ClientId&redirect_uri=$([Uri]::EscapeDataString($redirect))&response_type=code&access_type=offline&prompt=select_account%20consent&scope=$scope"
$client = $listener.AcceptTcpClient()
$stream = $client.GetStream()
$requestLine = [IO.StreamReader]::new($stream).ReadLine()
$page = '<html><head><meta charset="utf-8"></head><body>SeedCore: ログインが完了しました。PowerShell へ戻ってください。</body></html>'
$bytes = [Text.Encoding]::UTF8.GetBytes("HTTP/1.1 200 OK`r`nContent-Type: text/html; charset=utf-8`r`nContent-Length: $([Text.Encoding]::UTF8.GetByteCount($page))`r`nConnection: close`r`n`r`n$page")
$stream.Write($bytes, 0, $bytes.Length)
$stream.Flush()
$client.Close()
$listener.Stop()
if ($requestLine -notmatch 'code=([^&\s]+)')
{
	throw "ログインに失敗しました: $requestLine"
}
$token = (Invoke-RestMethod -Method Post -Uri 'https://oauth2.googleapis.com/token' -Body @{ code = [Uri]::UnescapeDataString($Matches[1]); client_id = $ClientId; client_secret = $ClientSecret; redirect_uri = $redirect; grant_type = 'authorization_code' }).access_token
$headers = @{ Authorization = "Bearer $token" }
$owner = (Invoke-RestMethod -Uri 'https://www.googleapis.com/oauth2/v3/userinfo' -Headers $headers).email
Write-Host "  $owner としてログインしました"

# 同意画面は権限ごとのチェックボックスで、外したまま進める。
# 足りないまま先へ進むと分かりにくい 403 になるので、ここで止める。
$granted = (Invoke-RestMethod -Uri "https://oauth2.googleapis.com/tokeninfo?access_token=$token").scope
foreach ($required in @('https://www.googleapis.com/auth/drive', 'https://www.googleapis.com/auth/documents'))
{
	if ($granted -notlike "*$required*")
	{
		throw "権限が足りません ($required)。同意画面でチェックを入れ直してください。`n許可された権限: $granted"
	}
}

function Send-Json([string]$Method, [string]$Uri, $Body)
{
	$parameters = @{ Method = $Method; Uri = $Uri; Headers = $headers; TimeoutSec = 60 }
	if ($Body)
	{
		$parameters.Body = [Text.Encoding]::UTF8.GetBytes(($Body | ConvertTo-Json -Compress -Depth 8))
		$parameters.ContentType = 'application/json; charset=utf-8'
	}
	try
	{
		return Invoke-RestMethod @parameters
	}
	catch
	{
		# Google はエラーの理由を本文に書くので、それを見せないと何が足りないのか分からない。
		$response = $_.Exception.Response
		if ($response)
		{
			$text = [IO.StreamReader]::new($response.GetResponseStream()).ReadToEnd()
			throw "HTTP $([int]$response.StatusCode) $Method $Uri`n$text"
		}
		throw
	}
}

function New-Folder([string]$Name, [string]$ParentId)
{
	# 同じ名前のフォルダが既にあれば作り直さない。2回流しても増えないようにするため。
	$query = [Uri]::EscapeDataString("name = '$Name' and mimeType = 'application/vnd.google-apps.folder' and trashed = false" + $(if ($ParentId) { " and '$ParentId' in parents" } else { '' }))
	$found = Send-Json 'Get' "$drive`?q=$query&fields=files(id,name)&pageSize=1"
	if ($found.files.Count -gt 0)
	{
		Write-Host "  フォルダ $Name は既にあります ($($found.files[0].id))"
		return $found.files[0].id
	}

	$body = @{ name = $Name; mimeType = 'application/vnd.google-apps.folder' }
	if ($ParentId)
	{
		$body.parents = @($ParentId)
	}
	$folder = Send-Json 'Post' "$drive`?fields=id" $body
	Write-Host "  フォルダ $Name を作成しました ($($folder.id))"
	return $folder.id
}

function New-Document([string]$Title, [string]$ParentId)
{
	# ドキュメントは Docs API で作られ、作成者のマイドライブ直下に置かれる。
	# その後 Drive API で共有フォルダへ移す。
	$query = [Uri]::EscapeDataString("name = '$Title' and '$ParentId' in parents and trashed = false")
	$found = Send-Json 'Get' "$drive`?q=$query&fields=files(id,name)&pageSize=1"
	if ($found.files.Count -gt 0)
	{
		Write-Host "  ドキュメント $Title は既にあります ($($found.files[0].id))"
		return $found.files[0].id
	}

	$document = Send-Json 'Post' $docs @{ title = $Title }
	Send-Json 'Patch' "$drive/$($document.documentId)?addParents=$ParentId&removeParents=root&fields=id" @{} | Out-Null
	Write-Host "  ドキュメント $Title を作成しました ($($document.documentId))"
	return $document.documentId
}

# --- library ------------------------------------------------------------
Write-Host ''
Write-Host '共有ライブラリを用意しています...'
$libraryId = New-Folder $LibraryName $null
$blobFolderId = New-Folder 'blobs' $libraryId
$assetsFolderId = New-Folder 'Assets' $libraryId
$catalogDocumentId = New-Document 'catalog' $libraryId
$lockDocumentId = New-Document 'lock' $libraryId

# --- members ------------------------------------------------------------
foreach ($member in $Members)
{
	# フォルダの共有は中のファイルへ引き継がれるので、フォルダに1回付ければ足りる。
	Send-Json 'Post' "$drive/$libraryId/permissions?sendNotificationEmail=false&fields=id" @{ type = 'user'; role = 'writer'; emailAddress = $member } | Out-Null
	Write-Host "  $member を共有相手に追加しました"
}

# --- configuration ------------------------------------------------------
$settings = [ordered]@{
	clientId = $ClientId
	clientSecret = $ClientSecret
	catalogDocumentId = $catalogDocumentId
	lockDocumentId = $lockDocumentId
	blobFolderId = $blobFolderId
	assetsFolderId = $assetsFolderId
	workspace = $Workspace
	owner = ''
}

[IO.File]::WriteAllText($path, ($settings | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))

Write-Host ''
Write-Host "設定を書き出しました: $path"
Write-Host ''
Write-Host '次にやること:'
Write-Host '  1. このファイルをチームのメンバーへ配る'
Write-Host '     (.asset は .gitignore に入っているので、リポジトリには乗りません)'
Write-Host '  2. メンバーは同じ Startup.bat を実行して 2 を選び、もらったファイルを指定します'
Write-Host '  3. Editor を起動すると、各自初回だけブラウザで Google ログインを求められます'
Write-Host '  4. owner は空のままで構いません。空のときは Windows のアカウント名が使われます'
