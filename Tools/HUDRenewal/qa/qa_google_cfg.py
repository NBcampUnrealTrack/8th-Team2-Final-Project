# Read-only: are the Google OAuth values loaded from DefaultGame.ini? (the secret is never printed, only its length)
import unreal
s = unreal.get_default_object(unreal.load_class(None, '/Script/ProjectM.NUGoogleAuthSettings'))
cid = s.get_editor_property('client_id')
sec = s.get_editor_property('client_secret')
log('GOOGLE CFG client_id=%s secret_len=%d verify=%s widget=%s' % (cid, len(sec), s.get_editor_property('server_verifies_id_token'),
                                                                   s.get_editor_property('login_widget_override')))
