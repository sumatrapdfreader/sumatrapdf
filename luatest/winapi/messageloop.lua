--class/messageloop: the thread message loop function.
setfenv(1, require'winapi')
require'winapi.window'
require'winapi.accelerator'
require'winapi.windowclass'

WM_UNREGISTER_CLASS = WM_APP+1 --unregister window class after window destruction

function ProcessMessage(msg)
	local window = Windows.active_window
	if window then
		if window.accelerators and window.accelerators.haccel then
			if TranslateAccelerator(window.hwnd, window.accelerators.haccel, msg) then --make hotkeys work
				return
			end
		end
		if not window.__wantallkeys then --TODO: make WM_GETDLGCODE -> DLGC_WANTALLKEYS work instead
			if IsDialogMessage(window.hwnd, msg) then --make tab and arrow keys work with controls
				return
			end
		end
	end
	TranslateMessage(msg) --make keyboard work
	DispatchMessage(msg) --make everything else work
end

function MessageLoop(after_process) --you can do os.exit(MessageLoop())
	local msg = types.MSG()
	while true do
		local ret = GetMessage(nil, 0, 0, msg)
		if ret == 0 then break end
		ProcessMessage(msg)
		if msg.message == WM_UNREGISTER_CLASS then
			UnregisterClass(msg.wParam)
		end
		if after_process then
			after_process(msg)
		end
	end
	return msg.signed_wParam --WM_QUIT returns 0 and an int exit code in wParam
end

function ProcessMessages()
	while true do
		local ok, msg = PeekMessage(nil, 0, 0, PM_REMOVE)
		if not ok then return end
		ProcessMessage(msg)
	end
end

