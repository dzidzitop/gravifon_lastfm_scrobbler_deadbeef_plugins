Gravifon scrobbler plugin for DeaDBeeF
======================================

Gravifon scrobbler plugin for the audio player DeaDBeeF.

Configuration
-------------

<table border="1">
<thead>
	<tr><th>Setting</th><th>Description</th><th>Default value</th></tr>
<thead>
<tbody>
	<tr><td>Enable scrobbler</td>
		<td>Activates the plugin so that is collects and submits scrobbles.
			No existing scrobbles are discarded if the plugin is disabled.
			However, they are not submitted.
			<br/>
			The plugin is activated or de-activated with the next scrobble or start/stop
			of the DeaDBeeF instance.
			</td>
		<td>Opted out (the plugin is not activated)</td></tr>
	<tr><td>Username</td>
		<td>The user name to submit scrobbles to Gravifon against.
			It must contain only ASCII characters (refer to the Gravifon documentation
			for more details).</td>
		<td><em>an empty string</em></td></tr>
	<tr><td>Password</td>
		<td>The password to authenticate the user with.
			It must contain only ASCII characters (refer to the Gravifon documentation
			for more details).</td>
		<td><em>an empty string</em></td></tr>
	<tr><td>URL to Gravifon API</td>
		<td>The URL to Gravifon services. It must be defined for gravifon to submit scrobbles
			(scrobbles are collected in either case).</td>
		<td><code>http://api.gravifon.org/v1</code></td></tr>
	<tr><td>Scrobble threshold (%)</td>
		<td>Defines what percentage of the track should be played for this track
			to be collected and scrobbled by the plugin. The valid values are from
			<code>0</code> to <code>100</code> (a decimal number), where
			<code>0</code> means that a track with any play duration is scrobbled, and
			<code>100</code> means that the track play duration must be not less than
			the track duration for this track to be scrobbled. An invalid value set is
			reset to <code>0.0</code>.
			<br/>
			Note that DeaDBeeF calculates track and play durations inaccurately so it is
			dangerous to set it to values that are close to 100. This could lead to some
			tracks not scrobbled even if they are played long enough.</td>
		<td><code>0.0</code></td></tr>
	<tr><td>Failure-safe scrobbling</td>
		<td>Activates a mode in which the plugin saves each scrobble to the data file once
			a scrobble event arises. This allows pending scrobbles to be recovered even
			in case of an emergency (power blackout, DeaDBeeF crash, etc.) at the expense
			of slightly slower performance and continuous disk writes. All existing pending
			scrobbles stored in the data file are preserved in this mode.
			<br/>
			If a scrobble event arises when the plugin is not in this mode then there is no
			way to store it to the data file other than to de-activate the plugin (either
			by disabling it or by closing the DeaDBeeF instance).
			<br/>
			By default pending scrobbles are stored in memory only.</td>
		<td>Opted out (failure-safe scrobbling is disabled)</td></tr>
	<tr><td><em>the data file</em> (non-configurable)</td>
		<td>The data file contains pending scrobbles, i.e. the track plays that are to be
			scrobbled by Gravifon but still not processed.
			When the plugin is activated it loads the pending scrobbles from the data file.
			When the plugin is de-activated the plugin rewrites the data file with the
			pending scrobbles that are stored in memory. If the failure-safe mode is enabled
			then the data file is used to store new pending scrobbles (they are appended to
			the existing ones).
			<br/>
			The data file is sought by the plugin using the following scheme. If <code>$XDG_DATA_HOME</code>
			is defined then the data file path is <code>$XDG_DATA_HOME/deadbeef/gravifon_scrobbler_data</code>.
			If <code>$XDG_DATA_HOME</code> is undefined then <code>$HOME</code> must be defined.
			In this case the data file path is <code>$HOME/.local/share/deadbeef/gravifon_scrobbler_data</code>.
			If both variables are undefined then the data file is unaccessible and the plugin activation fails.
			<br/>
			The data file is a text file which could copied or edited manually (with care).</td>
		<td>N/A</td></tr>
</tbody>
</table>

Build instruction (Unix-like systems)
-------------------------------------

Here, `${basedir}` denotes the root directory of the gravifon scrobbler codebase.

1. install the build tool [`ninja`](https://github.com/martine/ninja)
2. install GCC g++ 4.7+
3. install the libraries (including development versions; use your package manager for this):
    * `libjsoncpp`
    * `libcurl`
4. build the static version of the library [`libafc`](https://github.com/dzidzitop/libafc) and copy it to `${basedir}/lib`
5. copy headers of the library `libafc` to `${basedir}/include`
6. get the source code package of DeaDBeeF 0.5.6 and copy the file `deadbeef.h` to `${basedir}/include`
7. execute `ninja sharedLib` from `${basedir}`. The shared library `gravifon_scrobbler.so` will be created in `${basedir}/build`
8. copy `${basedir}/build/gravifon_scrobbler.so` to `$HOME/.local/lib/deadbeef`

System requirements
-------------------

* DeaDBeeF 0.5.6+
* GCC g++ 4.7+
* libcurl 7.26.0+
* libjsoncpp 0.5.0+
* ninja 1.3.3+
