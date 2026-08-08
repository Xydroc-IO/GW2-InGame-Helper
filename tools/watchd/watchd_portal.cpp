#include "watchd_internal.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/buffer/buffer.h>
#include <spa/pod/builder.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace WatchdDetail
{
	namespace
	{
		struct PwState
		{
			struct pw_thread_loop* loop = nullptr;
			struct pw_context* ctx = nullptr;
			struct pw_core* core = nullptr;
			struct pw_stream* stream = nullptr;
			struct spa_hook streamListener{};
			spa_video_info_raw format{};
			bool haveFormat = false;
			int spaFmt = SPA_VIDEO_FORMAT_UNKNOWN;
			uint32_t nodeId = 0;
		};

		void OnStreamParamChanged(void* data, uint32_t id, const struct spa_pod* param)
		{
			auto* st = static_cast<PwState*>(data);
			if (param == nullptr || id != SPA_PARAM_Format)
				return;
			uint32_t mediaType = 0, mediaSubtype = 0;
			if (spa_format_parse(param, &mediaType, &mediaSubtype) < 0)
				return;
			if (mediaType != SPA_MEDIA_TYPE_video || mediaSubtype != SPA_MEDIA_SUBTYPE_raw)
				return;
			if (spa_format_video_raw_parse(param, &st->format) < 0)
				return;
			st->spaFmt = static_cast<int>(st->format.format);
			st->haveFormat = true;
			std::fprintf(stderr, "watchd: negotiated %ux%u fmt=%d\n",
				st->format.size.width, st->format.size.height, st->spaFmt);
		}

		void OnStreamProcess(void* data)
		{
			auto* st = static_cast<PwState*>(data);
			if (!st->stream || !st->haveFormat || !gWantCapture.load())
				return;

			struct pw_buffer* b = pw_stream_dequeue_buffer(st->stream);
			if (!b)
				return;
			struct spa_buffer* buf = b->buffer;
			if (!buf || buf->n_datas < 1 || !buf->datas[0].data)
			{
				pw_stream_queue_buffer(st->stream, b);
				return;
			}

			const uint32_t w = st->format.size.width;
			const uint32_t h = st->format.size.height;
			uint32_t stride = buf->datas[0].chunk ? buf->datas[0].chunk->stride : 0;
			if (stride == 0)
				stride = w * 4;
			PublishSpaFrame(w, h, stride, static_cast<const uint8_t*>(buf->datas[0].data), st->spaFmt);
			pw_stream_queue_buffer(st->stream, b);
		}

		struct pw_stream_events kStreamEvents{};

		bool RunPipeWire(int pwFd, uint32_t nodeId, std::string& errOut)
		{
			kStreamEvents = {};
			kStreamEvents.version = PW_VERSION_STREAM_EVENTS;
			kStreamEvents.param_changed = OnStreamParamChanged;
			kStreamEvents.process = OnStreamProcess;

			PwState st{};
			st.nodeId = nodeId;
			pw_init(nullptr, nullptr);
			st.loop = pw_thread_loop_new("gw2igh-watchd", nullptr);
			if (!st.loop)
			{
				errOut = "pw_thread_loop_new failed";
				return false;
			}
			pw_thread_loop_lock(st.loop);
			st.ctx = pw_context_new(pw_thread_loop_get_loop(st.loop), nullptr, 0);
			if (!st.ctx)
			{
				errOut = "pw_context_new failed";
				pw_thread_loop_unlock(st.loop);
				pw_thread_loop_destroy(st.loop);
				return false;
			}
			st.core = pw_context_connect_fd(st.ctx, pwFd, nullptr, 0);
			if (!st.core)
			{
				errOut = "pw_context_connect_fd failed";
				pw_context_destroy(st.ctx);
				pw_thread_loop_unlock(st.loop);
				pw_thread_loop_destroy(st.loop);
				return false;
			}

			st.stream = pw_stream_new(st.core, "gw2igh-watch",
				pw_properties_new(
					PW_KEY_MEDIA_TYPE, "Video",
					PW_KEY_MEDIA_CATEGORY, "Capture",
					PW_KEY_MEDIA_ROLE, "Camera",
					nullptr));
			if (!st.stream)
			{
				errOut = "pw_stream_new failed";
				pw_core_disconnect(st.core);
				pw_context_destroy(st.ctx);
				pw_thread_loop_unlock(st.loop);
				pw_thread_loop_destroy(st.loop);
				return false;
			}
			pw_stream_add_listener(st.stream, &st.streamListener, &kStreamEvents, &st);

			uint8_t buffer[1024];
			struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
			const struct spa_pod* params[1];
			spa_rectangle defRect = SPA_RECTANGLE(640, 360);
			spa_rectangle minRect = SPA_RECTANGLE(1, 1);
			spa_rectangle maxRect = SPA_RECTANGLE(4096, 4096);
			spa_fraction defFps = SPA_FRACTION(60, 1);
			spa_fraction minFps = SPA_FRACTION(0, 1);
			spa_fraction maxFps = SPA_FRACTION(60, 1);
			params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&b,
				SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
				SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
				SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
				SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(8,
					SPA_VIDEO_FORMAT_BGRx,
					SPA_VIDEO_FORMAT_BGRx,
					SPA_VIDEO_FORMAT_BGRA,
					SPA_VIDEO_FORMAT_RGBx,
					SPA_VIDEO_FORMAT_RGBA,
					SPA_VIDEO_FORMAT_xRGB,
					SPA_VIDEO_FORMAT_ARGB,
					SPA_VIDEO_FORMAT_ABGR),
				SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(
					&defRect, &minRect, &maxRect),
				SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(
					&defFps, &minFps, &maxFps)));

			if (pw_stream_connect(st.stream, PW_DIRECTION_INPUT, nodeId,
					static_cast<pw_stream_flags>(
						PW_STREAM_FLAG_AUTOCONNECT |
						PW_STREAM_FLAG_MAP_BUFFERS),
					params, 1) < 0)
			{
				errOut = "pw_stream_connect failed";
				pw_stream_destroy(st.stream);
				pw_core_disconnect(st.core);
				pw_context_destroy(st.ctx);
				pw_thread_loop_unlock(st.loop);
				pw_thread_loop_destroy(st.loop);
				return false;
			}

			pw_thread_loop_start(st.loop);
			pw_thread_loop_unlock(st.loop);
			SetCapturing(true);
			std::fprintf(stderr, "watchd: PipeWire streaming node %u\n", nodeId);

			while (gWantCapture.load())
				g_usleep(50 * 1000);

			pw_thread_loop_lock(st.loop);
			pw_stream_destroy(st.stream);
			pw_core_disconnect(st.core);
			pw_context_destroy(st.ctx);
			pw_thread_loop_unlock(st.loop);
			pw_thread_loop_stop(st.loop);
			pw_thread_loop_destroy(st.loop);
			SetCapturing(false);
			return true;
		}

		std::string MakeToken(const char* prefix)
		{
			static uint32_t n = 0;
			++n;
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%s%u", prefix, n);
			return buf;
		}

		std::string SenderName(GDBusConnection* conn)
		{
			const char* unique = g_dbus_connection_get_unique_name(conn);
			if (!unique || unique[0] != ':')
				return "unknown";
			std::string s(unique + 1);
			for (char& c : s)
			{
				if (c == '.')
					c = '_';
			}
			return s;
		}

		bool CallPortal(GDBusConnection* conn, const char* method, GVariant* params,
			const char* handleToken, GVariant** resultsOut, std::string& errOut)
		{
			const std::string sender = SenderName(conn);
			const std::string reqPath = "/org/freedesktop/portal/desktop/request/" +
				sender + "/" + handleToken;

			guint32 response = 1;
			GVariant* results = nullptr;

			/* Subscribe before call so we cannot miss Response. */
			struct WaitCtx
			{
				GMainLoop* loop = nullptr;
				guint32 response = 1;
				GVariant* results = nullptr;
				bool done = false;
			} wait{};
			wait.loop = g_main_loop_new(nullptr, FALSE);
			auto onResponse = [](GDBusConnection*, const gchar*, const gchar*, const gchar*,
				const gchar*, GVariant* parameters, gpointer user) {
				auto* w = static_cast<WaitCtx*>(user);
				guint32 response = 1;
				GVariant* results = nullptr;
				g_variant_get(parameters, "(u@a{sv})", &response, &results);
				w->response = response;
				w->results = results;
				w->done = true;
				g_main_loop_quit(w->loop);
			};
			const guint sid = g_dbus_connection_signal_subscribe(conn,
				"org.freedesktop.portal.Desktop",
				"org.freedesktop.portal.Request",
				"Response",
				reqPath.c_str(),
				nullptr,
				G_DBUS_SIGNAL_FLAGS_NONE,
				onResponse,
				&wait,
				nullptr);

			GError* error = nullptr;
			GVariant* ret = g_dbus_connection_call_sync(conn,
				"org.freedesktop.portal.Desktop",
				"/org/freedesktop/portal/desktop",
				"org.freedesktop.portal.ScreenCast",
				method,
				params,
				G_VARIANT_TYPE("(o)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				nullptr,
				&error);
			if (!ret)
			{
				errOut = error ? error->message : "portal call failed";
				if (error)
					g_error_free(error);
				g_dbus_connection_signal_unsubscribe(conn, sid);
				g_main_loop_unref(wait.loop);
				return false;
			}
			g_variant_unref(ret);

			const gint64 deadline = g_get_monotonic_time() + 180 * G_TIME_SPAN_SECOND;
			while (!wait.done && g_get_monotonic_time() < deadline)
			{
				while (g_main_context_iteration(nullptr, FALSE))
				{}
				g_usleep(20 * 1000);
			}
			g_dbus_connection_signal_unsubscribe(conn, sid);
			g_main_loop_unref(wait.loop);

			if (!wait.done)
			{
				errOut = std::string(method) + " timed out (approve the portal dialog).";
				return false;
			}
			if (wait.response != 0)
			{
				errOut = std::string(method) + " cancelled or denied.";
				if (wait.results)
					g_variant_unref(wait.results);
				return false;
			}
			response = wait.response;
			results = wait.results;
			(void)response;
			if (resultsOut)
				*resultsOut = results;
			else if (results)
				g_variant_unref(results);
			return true;
		}

		int OpenPipeWireRemote(GDBusConnection* conn, const char* sessionPath, std::string& errOut)
		{
			GError* error = nullptr;
			GUnixFDList* fdList = nullptr;
			GVariant* ret = g_dbus_connection_call_with_unix_fd_list_sync(conn,
				"org.freedesktop.portal.Desktop",
				"/org/freedesktop/portal/desktop",
				"org.freedesktop.portal.ScreenCast",
				"OpenPipeWireRemote",
				g_variant_new("(oa{sv})", sessionPath, nullptr),
				G_VARIANT_TYPE("(h)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				nullptr,
				&fdList,
				nullptr,
				&error);
			if (!ret)
			{
				errOut = error ? error->message : "OpenPipeWireRemote failed";
				if (error)
					g_error_free(error);
				return -1;
			}
			gint32 handle = -1;
			g_variant_get(ret, "(h)", &handle);
			g_variant_unref(ret);
			if (!fdList || handle < 0)
			{
				errOut = "No PipeWire fd from portal.";
				if (fdList)
					g_object_unref(fdList);
				return -1;
			}
			const int fd = g_unix_fd_list_get(fdList, handle, &error);
			g_object_unref(fdList);
			if (fd < 0)
			{
				errOut = error ? error->message : "fd list get failed";
				if (error)
					g_error_free(error);
				return -1;
			}
			return fd;
		}
	}

	bool RunPortalCaptureLoop(std::string& errOut)
	{
		gPortalBusy = true;
		EnsureShm();

		GError* error = nullptr;
		GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
		if (!conn)
		{
			errOut = error ? error->message : "session bus failed";
			if (error)
				g_error_free(error);
			gPortalBusy = false;
			return false;
		}

		const std::string sessionToken = MakeToken("gw2ighs");
		const std::string handleToken = MakeToken("gw2ighr");
		const std::string sender = SenderName(conn);
		const std::string sessionPath = "/org/freedesktop/portal/desktop/session/" +
			sender + "/" + sessionToken;

		GVariantBuilder opt;
		g_variant_builder_init(&opt, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&opt, "{sv}", "handle_token", g_variant_new_string(handleToken.c_str()));
		g_variant_builder_add(&opt, "{sv}", "session_handle_token",
			g_variant_new_string(sessionToken.c_str()));

		GVariant* createResults = nullptr;
		if (!CallPortal(conn, "CreateSession",
				g_variant_new("(a{sv})", &opt), handleToken.c_str(), &createResults, errOut))
		{
			g_object_unref(conn);
			gPortalBusy = false;
			return false;
		}
		if (createResults)
			g_variant_unref(createResults);

		/* SelectSources: WINDOW | MONITOR */
		const std::string handle2 = MakeToken("gw2ighr");
		GVariantBuilder opt2;
		g_variant_builder_init(&opt2, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&opt2, "{sv}", "handle_token", g_variant_new_string(handle2.c_str()));
		g_variant_builder_add(&opt2, "{sv}", "types", g_variant_new_uint32(1u | 2u)); /* monitor|window */
		g_variant_builder_add(&opt2, "{sv}", "multiple", g_variant_new_boolean(FALSE));
		g_variant_builder_add(&opt2, "{sv}", "cursor_mode", g_variant_new_uint32(2u)); /* embedded */

		GVariant* selectResults = nullptr;
		if (!CallPortal(conn, "SelectSources",
				g_variant_new("(oa{sv})", sessionPath.c_str(), &opt2),
				handle2.c_str(), &selectResults, errOut))
		{
			g_object_unref(conn);
			gPortalBusy = false;
			return false;
		}
		if (selectResults)
			g_variant_unref(selectResults);

		const std::string handle3 = MakeToken("gw2ighr");
		GVariantBuilder opt3;
		g_variant_builder_init(&opt3, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add(&opt3, "{sv}", "handle_token", g_variant_new_string(handle3.c_str()));

		GVariant* startResults = nullptr;
		if (!CallPortal(conn, "Start",
				g_variant_new("(osa{sv})", sessionPath.c_str(), "", &opt3),
				handle3.c_str(), &startResults, errOut))
		{
			g_object_unref(conn);
			gPortalBusy = false;
			return false;
		}

		uint32_t nodeId = 0;
		if (startResults)
		{
			GVariant* streams = nullptr;
			if (g_variant_lookup(startResults, "streams", "@a(ua{sv})", &streams) && streams)
			{
				GVariantIter iter;
				g_variant_iter_init(&iter, streams);
				GVariant* child = nullptr;
				if ((child = g_variant_iter_next_value(&iter)) != nullptr)
				{
					guint32 nid = 0;
					GVariant* props = nullptr;
					g_variant_get(child, "(u@a{sv})", &nid, &props);
					nodeId = nid;
					if (props)
						g_variant_unref(props);
					g_variant_unref(child);
				}
				g_variant_unref(streams);
			}
			g_variant_unref(startResults);
		}
		if (nodeId == 0)
		{
			errOut = "Portal Start returned no PipeWire node.";
			g_object_unref(conn);
			gPortalBusy = false;
			return false;
		}

		const int pwFd = OpenPipeWireRemote(conn, sessionPath.c_str(), errOut);
		if (pwFd < 0)
		{
			g_object_unref(conn);
			gPortalBusy = false;
			return false;
		}

		const bool ok = RunPipeWire(pwFd, nodeId, errOut);
		/* pw_context_connect_fd takes ownership of fd on success; on failure close. */
		if (!ok)
			::close(pwFd);

		/* Close session best-effort. */
		g_dbus_connection_call_sync(conn,
			"org.freedesktop.portal.Desktop",
			sessionPath.c_str(),
			"org.freedesktop.portal.Session",
			"Close",
			nullptr,
			nullptr,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			nullptr,
			nullptr);

		g_object_unref(conn);
		gPortalBusy = false;
		SetCapturing(false);
		return ok;
	}
}
