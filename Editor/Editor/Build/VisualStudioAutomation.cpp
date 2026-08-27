#include <Editor/Editor/Build/VisualStudioAutomation.h>

namespace SeedCore
{
	Bool VisualStudioAutomation::InvokeMember(IDispatch* dispatch, const Wchar* name, WORD flags, VARIANT* arg, VARIANT* result)
	{
		if (!dispatch)
		{
			return false;
		}

		/// [EN] IDispatch::GetIDsOfNames takes a non-const OLECHAR** even though it never writes through it; the const_cast is only to satisfy that signature.
		/// [JP] IDispatch::GetIDsOfNames は非constな OLECHAR** を取るが、実際にはそこへ書き込むことはない。const_cast はそのシグネチャに合わせるためだけのもの。
		Wchar* nameNonConst = const_cast<Wchar*>(name);
		DISPID dispID = 0;
		if (FAILED(dispatch->GetIDsOfNames(IID_NULL, &nameNonConst, 1, LOCALE_USER_DEFAULT, &dispID)))
		{
			return false;
		}

		DISPPARAMS params{};
		DISPID putDispID = DISPID_PROPERTYPUT;
		if (arg)
		{
			params.rgvarg = arg;
			params.cArgs = 1;
			if (flags == DISPATCH_PROPERTYPUT)
			{
				params.rgdispidNamedArgs = &putDispID;
				params.cNamedArgs = 1;
			}
		}

		EXCEPINFO exceptionInfo{};
		HRESULT hr = dispatch->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, result, &exceptionInfo, nullptr);
		return SUCCEEDED(hr);
	}

	IDispatch* VisualStudioAutomation::GetDispatchProperty(IDispatch* dispatch, const Wchar* name)
	{
		VARIANT result;
		VariantInit(&result);
		if (!InvokeMember(dispatch, name, DISPATCH_PROPERTYGET, nullptr, &result))
		{
			VariantClear(&result);
			return nullptr;
		}

		IDispatch* propertyDispatch = (result.vt == VT_DISPATCH) ? result.pdispVal : nullptr;
		if (propertyDispatch)
		{
			propertyDispatch->AddRef();
		}
		VariantClear(&result);
		return propertyDispatch;
	}

	std::wstring VisualStudioAutomation::GetStringProperty(IDispatch* dispatch, const Wchar* name)
	{
		VARIANT result;
		VariantInit(&result);
		if (!InvokeMember(dispatch, name, DISPATCH_PROPERTYGET, nullptr, &result))
		{
			VariantClear(&result);
			return std::wstring();
		}

		std::wstring value = (result.vt == VT_BSTR && result.bstrVal) ? std::wstring(result.bstrVal) : std::wstring();
		VariantClear(&result);
		return value;
	}

	Int VisualStudioAutomation::GetInt32Property(IDispatch* dispatch, const Wchar* name)
	{
		VARIANT result;
		VariantInit(&result);
		if (!InvokeMember(dispatch, name, DISPATCH_PROPERTYGET, nullptr, &result))
		{
			VariantClear(&result);
			return 0;
		}

		if (result.vt != VT_I4)
		{
			if (FAILED(VariantChangeType(&result, &result, 0, VT_I4)))
			{
				VariantClear(&result);
				return 0;
			}
		}

		Int value = result.lVal;
		VariantClear(&result);
		return value;
	}

	IDispatch* VisualStudioAutomation::FindDTEForSolution(const std::filesystem::path& solutionPath)
	{
		std::wstring targetPath = solutionPath.wstring();

		IRunningObjectTable* rot = nullptr;
		if (FAILED(GetRunningObjectTable(0, &rot)))
		{
			return nullptr;
		}

		IEnumMoniker* enumMoniker = nullptr;
		if (FAILED(rot->EnumRunning(&enumMoniker)))
		{
			rot->Release();
			return nullptr;
		}

		IDispatch* matchedDTE = nullptr;

		IMoniker* moniker = nullptr;
		while (!matchedDTE && enumMoniker->Next(1, &moniker, nullptr) == S_OK)
		{
			IBindCtx* bindContext = nullptr;
			if (SUCCEEDED(CreateBindCtx(0, &bindContext)))
			{
				Wchar* displayName = nullptr;
				if (SUCCEEDED(moniker->GetDisplayName(bindContext, nullptr, &displayName)))
				{
					std::wstring monikerName(displayName);
					CoTaskMemFree(displayName);

					/// [EN] DTE monikers look like "!VisualStudio.DTE.17.0:1234" (version, then the owning process's PID) — matching the "!VisualStudio.DTE" prefix catches every VS version without hardcoding one.
					/// [JP] DTEのモニカ名は "!VisualStudio.DTE.17.0:1234"(バージョン、その後に所有プロセスのPID)のような形をしている — "!VisualStudio.DTE" という接頭辞で判定すれば、バージョンをハードコードせず全バージョンを拾える。
					if (monikerName.starts_with(L"!VisualStudio.DTE"))
					{
						IUnknown* unknown = nullptr;
						if (SUCCEEDED(rot->GetObject(moniker, &unknown)))
						{
							IDispatch* dte = nullptr;
							if (SUCCEEDED(unknown->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&dte))))
							{
								IDispatch* solution = GetDispatchProperty(dte, L"Solution");
								if (solution)
								{
									std::wstring fullName = GetStringProperty(solution, L"FullName");
									solution->Release();

									if (!fullName.empty() && _wcsicmp(fullName.c_str(), targetPath.c_str()) == 0)
									{
										matchedDTE = dte;
										dte = nullptr;
									}
								}

								if (dte)
								{
									dte->Release();
								}
							}
							unknown->Release();
						}
					}
				}
				bindContext->Release();
			}
			moniker->Release();
		}

		enumMoniker->Release();
		rot->Release();

		return matchedDTE;
	}

	IDispatch* VisualStudioAutomation::FindOrCreateFilter(IDispatch* owner, const std::wstring& name, const std::wstring& onDiskFiltersContent)
	{
		IDispatch* filters = GetDispatchProperty(owner, L"Filters");
		if (!filters)
		{
			return nullptr;
		}

		IDispatch* found = nullptr;
		Int filterCount = GetInt32Property(filters, L"Count");

		for (Int index = 1; !found && index <= filterCount; ++index)
		{
			VARIANT indexArg;
			VariantInit(&indexArg);
			indexArg.vt = VT_I4;
			indexArg.lVal = index;

			VARIANT itemResult;
			VariantInit(&itemResult);
			if (InvokeMember(filters, L"Item", DISPATCH_METHOD, &indexArg, &itemResult) && itemResult.vt == VT_DISPATCH && itemResult.pdispVal)
			{
				std::wstring candidateName = GetStringProperty(itemResult.pdispVal, L"Name");
				if (candidateName == name)
				{
					found = itemResult.pdispVal;
					found->AddRef();
				}
			}
			VariantClear(&itemResult);
		}

		if (!found)
		{
			/// [EN] The on-disk file already having this exact Include="name" is a strong (not perfect — a name could theoretically appear as some unrelated attribute value elsewhere, but Include="X" for a Filter is a distinctive enough substring in practice) signal that Visual Studio's live model just hasn't caught up with a change made outside it. Creating the filter here anyway would write a second <Filter> node for the same name.
			/// [JP] ディスク上のファイルに既にこの Include="name" が存在するというのは、Visual Studioのライブなモデルが外部での変更にまだ追いついていないだけ、という強いシグナルになる(完璧ではない — 理論上どこか無関係な属性値として同じ文字列が出現しうるが、フィルタの Include="X" は実用上十分に特徴的な部分文字列)。それでもここでフィルタを作成してしまうと、同じ名前の <Filter> ノードが2つ目書き込まれてしまう。
			std::wstring includeNeedle = L"Include=\"" + name + L"\"";
			if (onDiskFiltersContent.contains(includeNeedle))
			{
				filters->Release();
				return nullptr;
			}

			VARIANT nameArg;
			VariantInit(&nameArg);
			nameArg.vt = VT_BSTR;
			nameArg.bstrVal = SysAllocString(name.c_str());

			VARIANT addResult;
			VariantInit(&addResult);
			if (InvokeMember(owner, L"AddFilter", DISPATCH_METHOD, &nameArg, &addResult) && addResult.vt == VT_DISPATCH && addResult.pdispVal)
			{
				found = addResult.pdispVal;
				found->AddRef();
			}
			VariantClear(&addResult);
			VariantClear(&nameArg);
		}

		filters->Release();
		return found;
	}

	IDispatch* VisualStudioAutomation::EnsureFilterChain(IDispatch* vcProject, const std::wstring& relativeDirectory, const std::wstring& onDiskFiltersContent)
	{
		if (relativeDirectory.empty())
		{
			vcProject->AddRef();
			return vcProject;
		}

		IDispatch* current = vcProject;
		Bool ownsCurrent = false;

		Size segmentStart = 0;
		while (segmentStart <= relativeDirectory.size())
		{
			Size separatorIndex = relativeDirectory.find(L'\\', segmentStart);
			std::wstring segment = relativeDirectory.substr(segmentStart, (separatorIndex == std::wstring::npos) ? std::wstring::npos : separatorIndex - segmentStart);

			IDispatch* next = FindOrCreateFilter(current, segment, onDiskFiltersContent);
			if (ownsCurrent)
			{
				current->Release();
			}

			if (!next)
			{
				return nullptr;
			}

			current = next;
			ownsCurrent = true;

			if (separatorIndex == std::wstring::npos)
			{
				break;
			}
			segmentStart = separatorIndex + 1;
		}

		return current;
	}

	Bool VisualStudioAutomation::TryAddFilesToProject(const std::filesystem::path& solutionPath, const std::string& projectName, const std::filesystem::path& headerPath, const std::filesystem::path& cppPath)
	{
		Bool comInitializedHere = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

		Bool success = false;

		IDispatch* dte = FindDTEForSolution(solutionPath);
		if (dte)
		{
			IDispatch* solution = GetDispatchProperty(dte, L"Solution");
			if (solution)
			{
				IDispatch* projects = GetDispatchProperty(solution, L"Projects");
				if (projects)
				{
					IDispatch* project = nullptr;
					Int projectCount = GetInt32Property(projects, L"Count");
					std::wstring wideProjectName(projectName.begin(), projectName.end());

					for (Int index = 1; !project && index <= projectCount; ++index)
					{
						VARIANT indexArg;
						VariantInit(&indexArg);
						indexArg.vt = VT_I4;
						indexArg.lVal = index;

						VARIANT itemResult;
						VariantInit(&itemResult);
						if (InvokeMember(projects, L"Item", DISPATCH_METHOD, &indexArg, &itemResult) && itemResult.vt == VT_DISPATCH && itemResult.pdispVal)
						{
							std::wstring candidateName = GetStringProperty(itemResult.pdispVal, L"Name");
							if (candidateName == wideProjectName)
							{
								project = itemResult.pdispVal;
								project->AddRef();
							}
						}
						VariantClear(&itemResult);
					}

					if (project)
					{
						/// [EN] Project.ProjectItems.AddFromFile() (the generic EnvDTE path used previously) always adds at the project root with no <Filter> — Solution Explorer would show the file but never nested under Script/Script\Player like Tutorial's own files are. Project.Object exposes the native VC++ project model instead (VCProject, whose Filters really are the .vcxproj.filters <Filter> entries), so a file added through a VCFilter's own AddFile() lands in the vcxproj.filters with the matching <Filter> element already set.
						/// [JP] Project.ProjectItems.AddFromFile()(以前使っていた汎用EnvDTE経路)は常にプロジェクトのルートへ、<Filter>指定無しで追加してしまう — Solution Explorerには出るが、Tutorial自身のファイルのように Script/Script\Player 配下にネストされることは無い。代わりに Project.Object はネイティブなVC++プロジェクトモデル(VCProject。その Filters は .vcxproj.filters の <Filter> エントリそのもの)を公開しており、VCFilter自身の AddFile() 経由で追加したファイルは、対応する <Filter> 要素が最初から設定された状態で .vcxproj.filters に載る。
						IDispatch* vcProject = GetDispatchProperty(project, L"Object");
						if (vcProject)
						{
							std::filesystem::path projectRoot = solutionPath.parent_path().parent_path();
							std::filesystem::path userProjectRoot = projectRoot / "UserProject";
							std::error_code relativeError;
							std::filesystem::path relativeDirectoryPath = std::filesystem::relative(headerPath.parent_path(), userProjectRoot, relativeError);
							std::wstring relativeDirectory = (!relativeError && relativeDirectoryPath != ".") ? relativeDirectoryPath.wstring() : std::wstring();

							/// [EN] Read as raw bytes and widened one-to-one (not properly UTF-8 decoded) — fine here because every needle ever searched for (Include="Script", Include="Script\Player", ...) is pure ASCII, and UTF-8 encodes ASCII as single unchanged bytes with every multi-byte sequence's bytes all >= 0x80, so they can never coincide with an ASCII needle. A real UTF-8 decode isn't needed just to find one.
							/// [JP] 生バイトのまま1バイト=1文字として幅を広げて読む(正しいUTF-8デコードではない) — ここでは問題ない。探すのは Include="Script"、Include="Script\Player" のような純ASCIIの文字列だけであり、UTF-8はASCIIを変化させず1バイトのまま符号化し、マルチバイト列のバイトは全て0x80以上になるため、ASCIIの探索文字列と一致することは絶対に無い。1つの文字列を探すだけなら、正しいUTF-8デコードは不要。
							std::wstring onDiskFiltersContent;
							{
								std::ifstream filtersFile(userProjectRoot / "UserProject.vcxproj.filters", std::ios::binary);
								if (filtersFile)
								{
									std::string rawContent((std::istreambuf_iterator<Char>(filtersFile)), std::istreambuf_iterator<Char>());
									onDiskFiltersContent.assign(rawContent.begin(), rawContent.end());
								}
							}

							IDispatch* leafFilter = EnsureFilterChain(vcProject, relativeDirectory, onDiskFiltersContent);
							if (leafFilter)
							{
								Bool addedHeader = false;
								Bool addedCpp = false;

								for (const std::filesystem::path& path : { headerPath, cppPath })
								{
									VARIANT pathArg;
									VariantInit(&pathArg);
									pathArg.vt = VT_BSTR;
									pathArg.bstrVal = SysAllocString(path.wstring().c_str());

									VARIANT addResult;
									VariantInit(&addResult);
									Bool added = InvokeMember(leafFilter, L"AddFile", DISPATCH_METHOD, &pathArg, &addResult);
									VariantClear(&addResult);
									VariantClear(&pathArg);

									if (path == headerPath)
									{
										addedHeader = added;
									}
									else
									{
										addedCpp = added;
									}
								}

								success = addedHeader && addedCpp;
								leafFilter->Release();
							}
							vcProject->Release();
						}

						if (success)
						{
							/// [EN] AddFile() alone marks the project dirty in memory; Save() flushes that to UserProject.vcxproj/.filters on disk so the registration survives even if Visual Studio (or the whole machine) closes before the user's next manual save.
							/// [JP] AddFile() だけではプロジェクトがメモリ上でダーティになるだけなので、Save() で UserProject.vcxproj/.filters へ実際に書き出す。次にユーザーが手動保存するより前に Visual Studio(や PC 自体)が閉じても、登録内容が失われないようにするため。
							VARIANT emptyPath;
							VariantInit(&emptyPath);
							emptyPath.vt = VT_BSTR;
							emptyPath.bstrVal = SysAllocString(L"");
							VARIANT saveResult;
							VariantInit(&saveResult);
							/// [EN] Result intentionally ignored: Save() failing here just means the in-memory registration (already visible in Solution Explorer) hasn't been flushed to disk yet — a real problem, but not one this function can do anything more about, and the caller has no fallback that would help either (the registration already exists in Visual Studio's own model at this point).
							/// [JP] 結果は意図的に無視している: ここで Save() が失敗しても、メモリ上の登録(既にSolution Explorerには反映済み)がディスクへまだ書き出されていないというだけで、実害はあるがこの関数側でこれ以上できることは無く、呼び出し側にもこの時点では役立つフォールバックが無い(登録自体は既にVisual Studioのモデル内に存在するため)。
							Bool saved = InvokeMember(project, L"Save", DISPATCH_METHOD, &emptyPath, &saveResult);
							(void)saved;
							VariantClear(&saveResult);
							VariantClear(&emptyPath);
						}

						project->Release();
					}
					projects->Release();
				}
				solution->Release();
			}
			dte->Release();
		}

		if (comInitializedHere)
		{
			CoUninitialize();
		}

		return success;
	}
}
