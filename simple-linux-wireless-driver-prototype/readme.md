# Linux Wi-Fi Driver Tutorial: How to Write a Simple Linux Wireless Driver Prototype

Previously, we explored how to create a simple Linux device driver. Now it's time to talk about how to create a simple Linux Wi-Fi driver.

A dummy Wi-Fi driver is usually created solely for education purposes. However, such a driver also serves as a starting point for a robust Linux Wi-Fi driver. Once you create the simplest driver, you can add extra features to it and customize it to meet your needs.

In this article, we show you how to write drivers for Linux and achieve basic functionality by implementing a minimal interface. Our driver will be able to scan for a dummy Wi-Fi network, connect to it, and disconnect from it.

This article will be useful for developers who are already familiar with device drivers and know how to create kernel modules but want to learn how to design a customizable FullMAC Linux Wi-Fi driver prototype.

# Creating init and exit functions

Before we start developing Linux drivers, let's briefly overview three types of wireless driver configurations in Linux:

- Cfg80211 - The configuration API for 802.11 devices in Linux. It works together with FullMAC drivers, which also should implement the MAC Sublayer Management Entity (MLME).
- Mac80211 - A subsystem of the Linux kernel that works with soft-MAC/half-MAC wireless devices. MLME is mostly implemented by the kernel, at least for station mode (STA).
- WEXT - Stands for Wireless-Extensions, which is a driver API that was replaced by cfg80211. New drivers should no longer implement WEXT.

In this example, we create a dummpy FullMAC driver based on cfg80211 that only supports [STA mode](https://en.wikipedia.org/wiki/Station_(networking)). We decided to call our sample Linux device driver "navifly".

Writing a dummpy Wi-Fi driver involves four major steps and starts with creating `init` and `exit` functions, which are required by every driver.

We use the `init` function to allocate the context for our driver. The `exit` function, in turns, is used to clean it out.

```c
static int __init virtual_wifi_init(void)
{
    g_ctx = navifly_create_context();
    if (g_ctx != NULL)
    {
        sema_init(&g_ctx->sem, 1);
        INIT_WORK(&g_ctx->ws_connect, navifly_connect_routine);
        g_ctx->connecting_ssid[0] = 0;
        INIT_WORK(&g_ctx->ws_disconnect, navifly_disconnect_routine);
        g_ctx->disconnect_reason_code = 0;
        INIT_WORK(&g_ctx->ws_scan, navifly_scan_routine);
        g_ctx->scan_request = NULL;
    }
    return g_ctx == NULL;
}
```

Here's the code for our `exit` function:

```c
static void __exit virtual_wifi_exit(void)
{
    cancel_work_sync(&g_ctx->ws_connect);
    cancel_work_sync(&g_ctx->ws_disconnect);
    cancel_work_sync(&g_ctx->ws_scan);
    navifly_free(g_ctx);
}
```

Outside of `navifly_create_context()`, we initialize `work_structs` and variables such as `g_ctx->connecting_ssid` and `g_ctx->scan_request` to show the basic functionality of Linux Wi-Fi drivers. However, a full-value customized driver may not include these functions and may be require more complex and flexible structures in its context.

Let's take a look at the context for our simple driver:

```c
struct navifly_context
{
    struct wiphy *wiphy;
    struct net_device *ndev;
    struct semaphore sem;
    struct work_struct ws_connect;
    char connection_ssid[sizeof(SSID_DUMMY)];
    struct work_struct ws_disconnect;
    u16 disconnect_reason_code;
    struct work_struct ws_scan;
    struct cfg80211_scan_request *scan_request;
};
```

Now let's move to creating and initializing the context.

# Creating and initializing the context

The `wiphy` struct describes a physical wireless device. You can list all your physical wireless devices with the `iw list` command.

The `ndev` command shows network devices. There should be at least  one wireless device in your network. When implemented, the FullMAC driver can support several virtual network interfaces. The `net_device` structure together with the `wireless_dev` structure represents a wireless network device.

In our demo, the `wireless_dev` structure is stored in the private data of `net_device`:

```c
struct navifly_ndev_priv_context
{
    struct navifly_context *navi;
    struct wireless_dev wdev;
};
```

Also, the `ndev` private context stores a pointer to the `navifly` context. Each `net_device` should have its own `wireless_dev` context if the device is wireless. Other fields of the `navifly_context` structure for our prototype are described later in this article.

Let's now create the `navifly` context. To do that, we need to allocate and register the `wiphy` structure first.

Let's take a look at the `navifly_create_context()` function:

```c
static struct navifly_context *navifly_create_context(void)
{
    struct navifly_context *ret = NULL;
    struct navifly_wiphy_priv_context *wiphy_data = NULL;
    struct navifly_ndev_priv_context *ndev_data = NULL;
}
```

Here's how to allocate memory for the `navifly_context` structure:

```c
/* allocate for navifly context */
ret = kmalloc(sizeof(*ret), GFP_KERNEL);
if (!ret)
{
    goto l_error;
}
```

The next step is to initialize the context by allocating a new `wiphy` structure. Functions like `wiphy_new()` require the `cfg80211_ops` structure. This structure has a lot of functions that, when implemented, represent features of the wireless device.

The next argument of the `wiphy_new()` function is the size of the private data that will be allocated with the `wiphy` structure. Also, the `wiphy_new_nm()` function allows us to set the device name. By default, it's `phy%d(phy0, phy1, etc)`, but in our driver example we used the name `navifly`.

```c
ret->wiphy = wiphy_new_nm(&nvf_cfg_ops, sizeof(struct navifly_wiphy_priv_context), WIPHY_NAME);
if (ret->wiphy == NULL)
{
    goto l_error_wiphy;
}
```

Let's initialize the private data of the `wiphy` structure and set the `navifly` context so we can get the `navifly_context` parameter out of the `wiphy` structure:

```c
wiphy_data = wiphy_get_navi_context(ret->wiphy);
wiphy_data->navi = ret;
```

The following codes sets modes that our device can support. It can support several modes, which can be set using the bitwise OR operators. Our demo supports only STA mode.

```c
ret->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION);
```

The code below sets supported bands and channels. The `nf_band_2ghz` structure only describes channels 6. (We picked this channel randomly from the [the list of WLAN channels](https://en.wikipedia.org/wiki/List_of_WLAN_channels) for demo purposes.)

```c
ret->wiphy->bands[NL80211_BAND_2GHZ] = &nf_band_2ghz;
```

The following code is needed to set a value if the device supports scan requests. This value represents the maximum number of [SSIDs](https://en.wikipedia.org/wiki/Service_set_(802.11_network)) the device can scan.

```c
ret->wiphy->max_scan_ssids = 69;
```

Next, we use `wiphy_register` to register the `wiphy` structure in the system. If the `wiphy` structure is valid, a new device can be listed - for example, using the `iw list` command. The `wiphy` we've created has no network interface yet. However, we can already call functions that don't require a network interface, such as the `iw phy phy0 set` channel function.

```c
if (wiphy_register(ret->wiphy) < 0)
{
    goto l_error_wiphy_register;
}
```

At this point, we're done allocating the `wiphy` context and we move further to allocating the `net_device` context.

To set the Ethernet device, the `alloc_netdev()` function takes the size of the private data, the name of the network device, and the value that describes the name origin. The last argument of the `alloc_netdev()` function is a function that will be called during allocation. In most cases, using the defualt `ether_setup` function is enough.

```
ret->ndev = alloc_netdev(sizeof(*ndev_data), NDEV_NAME, NET_NAME_ENUM, ether_setup);
if (ret->ndev == NULL)
{
    goto l_error_alloc_ndev;
}
```

Next, we initialize the private data of the network device and set the `navifly` context pointer and the `wireless_dev` structure. But first, we need to set up the `wiphy` structure and the `net_device` pointers for the `wireless_dev` structure. Thanks to setting up the `ieee80211_ptr` pointer for the `net_device` structure, the system recognizes that the current `net_device` is wireless.

```c
ndev_data = ndev_get_navi_context(ret->ndev);
ndev_data->navi = ret;
ndev_data->wdev.wiphy = ret->wiphy;
ndev_data->wdev.netdev = ret->ndev;
ndev_data->wdev.iftype = NL80211_IFTYPE_STATION;
ret->ndev->ieee80211_ptr = &ndev_data->wdev;
```

Now we set up functions for `net_device`. The `net_device_opsnvf_ndev_ops`

<!-- WIP -->

# references

- https://www.apriorit.com/dev-blog/645-lin-linux-wi-fi-driver-tutorial-how-to-write-simple-linux-wireless-driver-prototype
