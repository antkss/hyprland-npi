#include "XWaylandManager.hpp"
#include "../Compositor.hpp"
#include "../events/Events.hpp"
#include "xdg-output-unstable-v1-protocol.h"
#include "../config/ConfigValue.hpp"

#define OUTPUT_MANAGER_VERSION                   3
#define OUTPUT_DONE_DEPRECATED_SINCE_VERSION     3
#define OUTPUT_DESCRIPTION_MUTABLE_SINCE_VERSION 3

CHyprXWaylandManager::CHyprXWaylandManager() {
#ifndef NO_XWAYLAND
    m_sWLRXWayland = wlr_xwayland_create(g_pCompositor->m_sWLDisplay, g_pCompositor->m_sWLRCompositor, 1);

    if (!m_sWLRXWayland) {
        Debug::log(ERR, "Couldn't start up the XWaylandManager because wlr_xwayland_create returned a nullptr!");
        return;
    }

    addWLSignal(&m_sWLRXWayland->events.ready, &Events::listen_readyXWayland, m_sWLRXWayland, "XWayland Manager");
    addWLSignal(&m_sWLRXWayland->events.new_surface, &Events::listen_surfaceXWayland, m_sWLRXWayland, "XWayland Manager");

    setenv("DISPLAY", m_sWLRXWayland->display_name, 1);

    Debug::log(LOG, "CHyprXWaylandManager started on display {}", m_sWLRXWayland->display_name);
#else
    unsetenv("DISPLAY"); // unset DISPLAY so that X11 apps do not try to start on a different/invalid DISPLAY
#endif
}

CHyprXWaylandManager::~CHyprXWaylandManager() {}

wlr_surface* CHyprXWaylandManager::getWindowSurface(CWindow* pWindow) {
    if (pWindow->m_bIsX11)
        return pWindow->m_uSurface.xwayland->surface;

    return pWindow->m_uSurface.xdg->surface;
}

void CHyprXWaylandManager::activateSurface(wlr_surface* pSurface, bool activate) {
    if (!pSurface)
        return;

    if (wlr_xdg_surface_try_from_wlr_surface(pSurface)) {
        if (const auto PSURF = wlr_xdg_surface_try_from_wlr_surface(pSurface); PSURF && PSURF->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
            wlr_xdg_toplevel_set_activated(PSURF->toplevel, activate);

    } else if (wlr_xwayland_surface_try_from_wlr_surface(pSurface)) {
        const auto XSURF = wlr_xwayland_surface_try_from_wlr_surface(pSurface);
        wlr_xwayland_surface_activate(XSURF, activate);

        if (activate && !XSURF->override_redirect)
            wlr_xwayland_surface_restack(XSURF, nullptr, XCB_STACK_MODE_ABOVE);
    }
}

void CHyprXWaylandManager::activateWindow(CWindow* pWindow, bool activate) {
    if (pWindow->m_bIsX11) {
        setWindowSize(pWindow, pWindow->m_vRealSize.value()); // update xwayland output pos

        if (activate) {
            wlr_xwayland_surface_set_minimized(pWindow->m_uSurface.xwayland, false);
            if (!pWindow->m_uSurface.xwayland->override_redirect)
                wlr_xwayland_surface_restack(pWindow->m_uSurface.xwayland, nullptr, XCB_STACK_MODE_ABOVE);
        }

        wlr_xwayland_surface_activate(pWindow->m_uSurface.xwayland, activate);
    } else
        wlr_xdg_toplevel_set_activated(pWindow->m_uSurface.xdg->toplevel, activate);

    if (activate) {
        g_pCompositor->m_pLastFocus  = getWindowSurface(pWindow);
        g_pCompositor->m_pLastWindow = pWindow;
    }

    if (!pWindow->m_bPinned)
        pWindow->m_pWorkspace->m_pLastFocusedWindow = pWindow;
}

void CHyprXWaylandManager::getGeometryForWindow(CWindow* pWindow, CBox* pbox) {
    if (pWindow->m_bIsX11) {
        const auto SIZEHINTS = pWindow->m_uSurface.xwayland->size_hints;

        if (SIZEHINTS && pWindow->m_iX11Type != 2) {
            pbox->x      = SIZEHINTS->x;
            pbox->y      = SIZEHINTS->y;
            pbox->width  = SIZEHINTS->width;
            pbox->height = SIZEHINTS->height;
        } else {
            pbox->x      = pWindow->m_uSurface.xwayland->x;
            pbox->y      = pWindow->m_uSurface.xwayland->y;
            pbox->width  = pWindow->m_uSurface.xwayland->width;
            pbox->height = pWindow->m_uSurface.xwayland->height;
        }
    } else {
        wlr_xdg_surface_get_geometry(pWindow->m_uSurface.xdg, pbox->pWlr());
        pbox->applyFromWlr();
    }
}

std::string CHyprXWaylandManager::getTitle(CWindow* pWindow) {
    try {
        if (pWindow->m_bIsX11) {
            if (!pWindow->m_bIsMapped)
                return "";

            if (pWindow->m_uSurface.xwayland && pWindow->m_uSurface.xwayland->title) {
                return std::string(pWindow->m_uSurface.xwayland->title);
            }
        } else if (pWindow->m_uSurface.xdg) {
            if (pWindow->m_bFadingOut || !pWindow->m_uSurface.xdg)
                return "";

            if (pWindow->m_uSurface.xdg->toplevel && pWindow->m_uSurface.xdg->toplevel->title) {
                return std::string(pWindow->m_uSurface.xdg->toplevel->title);
            }
        } else {
            return "";
        }
    } catch (...) { Debug::log(ERR, "Error in getTitle (probably null title)"); }

    return "";
}

std::string CHyprXWaylandManager::getAppIDClass(CWindow* pWindow) {
    try {
        if (pWindow->m_bIsX11) {
            if (!pWindow->m_bIsMapped)
                return "";

            if (pWindow->m_uSurface.xwayland && pWindow->m_uSurface.xwayland->_class) {
                return std::string(pWindow->m_uSurface.xwayland->_class);
            }
        } else if (pWindow->m_uSurface.xdg) {
            if (pWindow->m_bFadingOut || !pWindow->m_uSurface.xdg)
                return "";

            if (pWindow->m_uSurface.xdg->toplevel && pWindow->m_uSurface.xdg->toplevel->app_id) {
                return std::string(pWindow->m_uSurface.xdg->toplevel->app_id);
            }
        } else
            return "";
    } catch (std::logic_error& e) { Debug::log(ERR, "Error in getAppIDClass: {}", e.what()); }

    return "";
}

void CHyprXWaylandManager::sendCloseWindow(CWindow* pWindow) {
    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_close(pWindow->m_uSurface.xwayland);
    else
        wlr_xdg_toplevel_send_close(pWindow->m_uSurface.xdg->toplevel);
}

void CHyprXWaylandManager::setWindowSize(CWindow* pWindow, Vector2D size, bool force) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    const auto  PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    size = size.clamp(Vector2D{0, 0}, Vector2D{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()});

    // calculate pos
    // TODO: this should be decoupled from setWindowSize IMO
    Vector2D windowPos = pWindow->m_vRealPosition.value();

    if (pWindow->m_bIsX11 && PMONITOR) {
        windowPos = windowPos - PMONITOR->vecPosition; // normalize to monitor
        if (*PXWLFORCESCALEZERO)
            windowPos = windowPos * PMONITOR->scale;           // scale if applicable
        windowPos = windowPos + PMONITOR->vecXWaylandPosition; // move to correct position for xwayland
    }

    if (!force && ((pWindow->m_vPendingReportedSize == size && windowPos == pWindow->m_vReportedPosition) || (pWindow->m_vPendingReportedSize == size && !pWindow->m_bIsX11)))
        return;

    pWindow->m_vReportedPosition    = windowPos;
    pWindow->m_vPendingReportedSize = size;

    pWindow->m_fX11SurfaceScaledBy = 1.f;

    if (*PXWLFORCESCALEZERO && pWindow->m_bIsX11 && PMONITOR) {
        size                           = size * PMONITOR->scale;
        pWindow->m_fX11SurfaceScaledBy = PMONITOR->scale;
    }

    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, windowPos.x, windowPos.y, size.x, size.y);
    else
        pWindow->m_vPendingSizeAcks.push_back(std::make_pair<>(wlr_xdg_toplevel_set_size(pWindow->m_uSurface.xdg->toplevel, size.x, size.y), size.floor()));
}

void CHyprXWaylandManager::setWindowStyleTiled(CWindow* pWindow, uint32_t edgez) {
    if (pWindow->m_bIsX11)
        return;

    wlr_xdg_toplevel_set_tiled(pWindow->m_uSurface.xdg->toplevel, edgez);
    wlr_xdg_toplevel_set_maximized(pWindow->m_uSurface.xdg->toplevel, true);
}

wlr_surface* CHyprXWaylandManager::surfaceAt(CWindow* pWindow, const Vector2D& client, Vector2D& surface) {
    if (pWindow->m_bIsX11)
        return wlr_surface_surface_at(pWindow->m_uSurface.xwayland->surface, client.x, client.y, &surface.x, &surface.y);

    return wlr_xdg_surface_surface_at(pWindow->m_uSurface.xdg, client.x, client.y, &surface.x, &surface.y);
}

bool CHyprXWaylandManager::shouldBeFloated(CWindow* pWindow, bool pending) {
    if (pWindow->m_bIsX11) {
        for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++)
            if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
                pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"]) {

                if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
                    pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"])
                    pWindow->m_bX11ShouldntFocus = true;

                pWindow->m_bNoInitialFocus = true;
                return true;
            }

        if (pWindow->m_uSurface.xwayland->role) {
            try {
                std::string winrole = std::string(pWindow->m_uSurface.xwayland->role);
                if (winrole.contains("pop-up") || winrole.contains("task_dialog")) {
                    return true;
                }
            } catch (std::exception& e) { Debug::log(ERR, "Error in shouldBeFloated, winrole threw {}", e.what()); }
        }

        if (pWindow->m_uSurface.xwayland->modal) {
            pWindow->m_bIsModal = true;
            return true;
        }

        if (pWindow->m_iX11Type == 2)
            return true; // override_redirect

        const auto SIZEHINTS = pWindow->m_uSurface.xwayland->size_hints;
        if (SIZEHINTS && (pWindow->m_uSurface.xwayland->parent || ((SIZEHINTS->min_width == SIZEHINTS->max_width) && (SIZEHINTS->min_height == SIZEHINTS->max_height))))
            return true;
    } else {
        const auto PSTATE = pending ? &pWindow->m_uSurface.xdg->toplevel->pending : &pWindow->m_uSurface.xdg->toplevel->current;

        if ((PSTATE->min_width != 0 && PSTATE->min_height != 0 && (PSTATE->min_width == PSTATE->max_width || PSTATE->min_height == PSTATE->max_height)) ||
            pWindow->m_uSurface.xdg->toplevel->parent)
            return true;
    }

    return false;
}

void CHyprXWaylandManager::moveXWaylandWindow(CWindow* pWindow, const Vector2D& pos) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (!pWindow->m_bIsX11)
        return;

    wlr_xwayland_surface_configure(pWindow->m_uSurface.xwayland, pos.x, pos.y, pWindow->m_vRealSize.value().x, pWindow->m_vRealSize.value().y);
}

void CHyprXWaylandManager::checkBorders(CWindow* pWindow) {
    if (!pWindow->m_bIsX11)
        return;

    for (size_t i = 0; i < pWindow->m_uSurface.xwayland->window_type_len; i++) {
        if (pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"] ||
            pWindow->m_uSurface.xwayland->window_type[i] == HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"]) {

            pWindow->m_bX11DoesntWantBorders = true;
            return;
        }
    }

    if (pWindow->m_iX11Type == 2) {
        pWindow->m_bX11DoesntWantBorders = true;
    }
}

void CHyprXWaylandManager::setWindowFullscreen(CWindow* pWindow, bool fullscreen) {
    if (pWindow->m_bIsX11)
        wlr_xwayland_surface_set_fullscreen(pWindow->m_uSurface.xwayland, fullscreen);
    else
        wlr_xdg_toplevel_set_fullscreen(pWindow->m_uSurface.xdg->toplevel, fullscreen);

    if (pWindow->m_phForeignToplevel)
        wlr_foreign_toplevel_handle_v1_set_fullscreen(pWindow->m_phForeignToplevel, fullscreen);
}

Vector2D CHyprXWaylandManager::getMaxSizeForWindow(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return Vector2D(99999, 99999);

    if ((pWindow->m_bIsX11 && !pWindow->m_uSurface.xwayland->size_hints) || (!pWindow->m_bIsX11 && !pWindow->m_uSurface.xdg->toplevel) ||
        pWindow->m_sAdditionalConfigData.noMaxSize)
        return Vector2D(99999, 99999);

    auto MAXSIZE = pWindow->m_bIsX11 ? Vector2D(pWindow->m_uSurface.xwayland->size_hints->max_width, pWindow->m_uSurface.xwayland->size_hints->max_height) :
                                       Vector2D(pWindow->m_uSurface.xdg->toplevel->current.max_width, pWindow->m_uSurface.xdg->toplevel->current.max_height);

    if (MAXSIZE.x < 5)
        MAXSIZE.x = 99999;
    if (MAXSIZE.y < 5)
        MAXSIZE.y = 99999;

    return MAXSIZE;
}

Vector2D CHyprXWaylandManager::getMinSizeForWindow(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return Vector2D(0, 0);

    if ((pWindow->m_bIsX11 && !pWindow->m_uSurface.xwayland->size_hints) || (!pWindow->m_bIsX11 && !pWindow->m_uSurface.xdg->toplevel))
        return Vector2D(0, 0);

    auto MINSIZE = pWindow->m_bIsX11 ? Vector2D(pWindow->m_uSurface.xwayland->size_hints->min_width, pWindow->m_uSurface.xwayland->size_hints->min_height) :
                                       Vector2D(pWindow->m_uSurface.xdg->toplevel->current.min_width, pWindow->m_uSurface.xdg->toplevel->current.min_height);

    MINSIZE = MINSIZE.clamp({1, 1});

    return MINSIZE;
}

Vector2D CHyprXWaylandManager::xwaylandToWaylandCoords(const Vector2D& coord) {

    static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

    CMonitor*   pMonitor     = nullptr;
    double      bestDistance = __FLT_MAX__;
    for (auto& m : g_pCompositor->m_vMonitors) {
        const auto SIZ = *PXWLFORCESCALEZERO ? m->vecTransformedSize : m->vecSize;

        double     distance =
            vecToRectDistanceSquared(coord, {m->vecXWaylandPosition.x, m->vecXWaylandPosition.y}, {m->vecXWaylandPosition.x + SIZ.x - 1, m->vecXWaylandPosition.y + SIZ.y - 1});

        if (distance < bestDistance) {
            bestDistance = distance;
            pMonitor     = m.get();
        }
    }

    if (!pMonitor)
        return Vector2D{};

    // get local coords
    Vector2D result = coord - pMonitor->vecXWaylandPosition;
    // if scaled, unscale
    if (*PXWLFORCESCALEZERO)
        result = result / pMonitor->scale;
    // add pos
    result = result + pMonitor->vecPosition;

    return result;
}