import unreal
stage=globals().get('STAGE','stop')
if stage=='stop':
 if unreal.WidgetService.is_pie_running():print('STOP REQUEST',unreal.WidgetService.stop_pie())
 else:print('ALREADY STOPPED')
elif stage=='start':
 assert not unreal.WidgetService.is_pie_running();print('START REQUEST',unreal.WidgetService.start_pie())
else:print('PIE',unreal.WidgetService.is_pie_running())
