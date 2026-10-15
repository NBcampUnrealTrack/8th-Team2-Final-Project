import unreal
assert not unreal.WidgetService.is_pie_running()
unreal.ViewportService.set_game_view(True)
unreal.ViewportService.set_realtime(True)
unreal.ViewportService.set_camera_location(unreal.Vector(-59.4,-2245,1189))
unreal.ViewportService.set_camera_rotation(unreal.Rotator(pitch=-25,yaw=90,roll=0))
unreal.ViewportService.set_fov(32)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('SAVED: fixed front gameplay angle, realtime fire preview')
