<!DOCTYPE html>
<html lang="en">


<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Start Cheating | Evadus</title>


<link rel="icon" href="/assets/logo-0c823ace.png" type="image/x-icon">
<script type="module" crossorigin src="/assets/index-6f382144.js"></script>
<link rel="modulepreload" crossorigin href="/assets/modulepreload-polyfill-3cfb730f.js">
<link rel="modulepreload" crossorigin href="/assets/firebase-a6426b78.js">
<link rel="stylesheet" href="/assets/index-71bcb781.css">
</head>


<body>
<header class="header">
<div class="container">
<a href="#home">
<div class="logo-circle">
<img src="/assets/logo-0c823ace.png" alt="Evadus Logo" class="logo-image">
</div>
</a>
<nav class="nav">
<a href="#features" class="nav-link">Features</a>
<a href="#pricing" class="nav-link">Pricing</a>
<a href="#compatibility" class="nav-link">Compatibility</a>
<a href="#leaderboard" class="nav-link">Leaderboard</a>
<a href="#help" class="nav-link">Help</a>
</nav>
<div class="auth-buttons">
<div id="auth-links-container">
<button class="login-btn" onclick="location.href='./login.html'" style="display: flex; align-items: center; gap: 8px; padding: 5px 12px;">
<span>Login / Register</span>
</button>
</div>
<div id="user-profile-container" style="display: none;">
<img id="user-profile-photo" src="/assets/profile_icon-c7e42270.png" alt="User Profile" class="user-photo">
<div id="user-dropdown-menu" class="dropdown-menu">
<a href="./dashboard.html">Dashboard</a>
<button id="open-logout-modal-btn">Logout</button>
</div>
</div>
</div>
</div>
</header>
<div id="tsparticles"></div>
<section class="hero" id="home">
<div class="container">
<pre id="ascii-hero" aria-label="Evadus"></pre>


<p class="hacker-subtitle">
// The safest and most convenient way to pass any online examination<span class="blinking-cursor">_</span>
</p>
<div class="hero-buttons">
<button class="hacker-cta-btn mac-btn">
					<img src="/assets/apple_logo-64f88d57.png" alt="Apple" class="btn-logo-svg">
					<span>DOWNLOAD [MacOS]</span>
				</button>
<button class="hacker-cta-btn windows-btn" id="windows-download-btn">
					<img src="/assets/windows_logo-14176e2b.png" alt="Windows" class="btn-logo-svg">
					<span>DOWNLOAD [Windows 10/11]</span>
				</button>
</div>
</div>
</section>


<div class="news-headlines">
<div class="headline-item" id="headline-item-1">
<span class="headline-text"></span>
<span class="headline-source"></span>
</div>
<div class="headline-item" id="headline-item-2">
<span class="headline-text"></span>
<span class="headline-source"></span>
</div>
<div class="headline-item" id="headline-item-3">
<span class="headline-text"></span>
<span class="headline-source"></span>
</div>
</div>
<div id="logout-modal" class="modal">
<div class="modal-content hacker-modal-content">
<div class="terminal-header">
<div class="terminal-title">// SYSTEM_QUERY [SESSION_TERMINATION]</div>
<div class="terminal-buttons">
<span class="t-btn t-red"></span>
<span class="t-btn t-yellow"></span>
<span class="t-btn t-green"></span>
</div>
</div>
<div class="hacker-modal-body">
<span class="close-btn">×</span>
<h3 class="panel-title" data-text="CONFIRM_LOGOUT">CONFIRM_LOGOUT</h3>
<p class="hacker-modal-text">
<span class="cli-prompt">&gt;</span> Are you sure you want to logout?
</p>
<div class="hacker-modal-buttons">
<button class="hacker-modal-btn cancel-btn">[CANCEL]</button>
<button id="confirm-logout-btn" class="hacker-modal-btn confirm-btn">[CONFIRM]</button>
</div>
</div>
</div>
</div>
<section class="manifesto-section" id="ethics">
<h2 class="section-title manifesto-title" data-text="Fuck. this. system.">Fuck. this. system.</h2>
<div class="dedsec-container">
<div class="manifesto-terminal">
<div class="terminal-header">
<div class="terminal-title">//INCOMING_TRANSMISSION [VERSION 10.0.26100.6584]</div>
<div class="terminal-buttons">
<span class="t-btn t-red"></span>
<span class="t-btn t-yellow"></span>
<span class="t-btn t-green"></span>
</div>
</div>
<!-- The terminal body is now empty and will be filled by JavaScript -->
<div class="terminal-body" id="terminal-content">
</div>
</div>
</div>
</section>


<section class="features-section" id="features">
<div class="container">
<h2 class="section-title manifesto-title" data-text="TOOLS">TOOLS</h2>


<div class="hacker-features-container">
<div class="features-tabs">
<button class="features-tab" data-target="#feature-lite">[EVADUS LITE]</button>
<button class="features-tab active" data-target="#feature-evadus">[EVADUS CORE]</button>
<button class="features-tab" data-target="#feature-plus">[EVADUS+]</button>
</div>


<div class="features-panels">
<div id="feature-lite" class="feature-panel">


<div class="feature-list-alternating">
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/overlay_demo.jpg">
<source src="./public/assets/videos/evadus_lite/web_showcase.webm" type="video/webm">
<source src="/assets/web_showcase-79dcef3b.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Invisible Web Overlay</h4>
<p class="feature-description">Access any URL (ChatGPT, Google) in a hidden overlay. Send messages to other classmates, watch Youtube... your imagination is the only limit.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Invisible to Screen Recording/Sharing</h4>
<p class="feature-description">Invisible to all screen capture, recording, and remote monitoring. They see your exam; you see the answers.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/cloaking_demo.jpg">
<source src="./public/assets/videos/evadus_lite/screensharing_showcase.webm" type="video/webm">
<source src="/assets/screensharing_showcase-0596de47.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/polymorph_demo.jpg">
<source src="./public/assets/videos/evadus_lite/polymorphic_showcase.webm" type="video/webm">
<source src="/assets/polymorphic_showcase-97c1c807.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Polymorphic Engine</h4>
<p class="feature-description">Every download is unique, running as one of 6M+ random, legitimate processes. Impossible to detect via Task Manager.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/hotkey_demo.jpg">
<source src="./public/assets/videos/evadus_lite/hotkey_showcase.webm" type="video/webm">
<source src="/assets/hotkey_showcase-cd94e92e.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Undetectable Hotkeys</h4>
<p class="feature-description">Run everything with global shortcuts. Our hotkeys operate at the system level, making them invisible to other apps, keystroke loggers, and proctoring software.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Stealth Screenshot Tool</h4>
<p class="feature-description">Instantly capture any part of your screen without detection. You can drag your screenshot directly into websites (like ChatGPT).</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/screenshot_showcase.jpg">
<source src="./public/assets/videos/evadus_lite/screenshot_showcase.webm" type="video/webm">
<source src="/assets/screenshot_showcase-30eb3127.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
</div>
</div>
<div id="feature-evadus" class="feature-panel active">


<div class="feature-list-alternating">
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/clickthrough_demo.jpg">
<source src="./public/assets/videos/evadus_core/clickthrough_demo.webm" type="video/webm">
<source src="/assets/clickthrough_demo-695f15f6.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Click-Through and Invisible Overlay</h4>
<p class="feature-description">Click, type, and scroll on the overlay without your exam losing focus, whilst invisble to screen recording/sharing.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Polymorphic Engine</h4>
<p class="feature-description">Every download is unique, running as one of 6M+ random, legitimate processes. Impossible to detect via Task Manager.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/polymorph_demo.jpg">
<source src="./public/assets/videos/evadus_core/polymorph_demo.webm" type="video/webm">
<source src="/assets/polymorph_demo-499cb9fc.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/ai_engine_demo.jpg">
<source src="./public/assets/videos/evadus_core/ai_models_demo.webm" type="video/webm">
<source src="/assets/ai_models_demo-8820a6e5.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Top AI Models</h4>
<p class="feature-description">Instant answers from top AI models (Claude 4.6 Opus, GPT-5.2, Gemini 3 Pro, Grok 4 etc.).</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Rapid Fire Thoughts</h4>
<p class="feature-description">A constant stream of low-latency, live responses. Get the AI's real-time thoughts so you're never left without an answer. Perfect for live situations.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/rapid_fire_demo.jpg">
<source src="./public/assets/videos/evadus_core/rapid_fire_demo.webm" type="video/webm">
<source src="/assets/rapid_fire_demo-4ee128df.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Voice-to-Text Query</h4>
<p class="feature-description">Hands-free cheating. Use your voice to ask questions and get answers.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/voice_showcase.jpg">
<source src="./public/assets/videos/evadus_core/voice_showcase.webm" type="video/webm">
<source src="/assets/voice_showcase-4fe213ba.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/hotkey_demo.jpg">
<source src="./public/assets/videos/evadus_core/hotkey_demo.webm" type="video/webm">
<source src="/assets/hotkey_demo-f1e0283b.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Undetectable Hotkeys</h4>
<p class="feature-description">Run everything with global shortcuts. Our hotkeys operate at the system level, making them invisible to other apps, keystroke loggers, and proctoring software.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/ai_models.jpg">
<source src="./public/assets/videos/evadus_core/additional_context_demo.webm" type="video/webm">
<source src="/assets/additional_context_demo-9c9e8220.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Custom AI Prompts</h4>
<p class="feature-description">Take full control of the AI. Send your own custom prompts or select from a list of powerful, pre-configured presets to get the exact answer you need.</p>
</div>
</div>


</div>
</div>
<div id="feature-plus" class="feature-panel">
<div class="feature-list-alternating">
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/vm_freedom_demo.jpg">
<source src="./public/assets/videos/evadus_core/vm_freedom_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/vm_freedom_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Local RDP Wrapper V2 <span style="color: #ffbd2e; font-size: 0.8em;">[OPEN BETA]</span></h4>
<p class="feature-description">
<span style="color: #ffbd2e; font-weight: bold;">// WARNING: BETA_BUILD in effect.</span><br> Create a hidden, parallel user session on your own PC. This is <b>not</b> a VM or Cloud PC. It uses advanced RDP wrapping to run a second desktop instance locally, completely invisible to the primary session where the proctor lives.
</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Perfect HWID Match</h4>
<p class="feature-description">Because you are running on your own metal, every Hardware ID, Serial Number, and MAC Address matches your main system 1:1. The exam software sees your <b>real</b> PC, eliminating any "Virtual Machine" detection flags.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/spoofing_demo.jpg">
<source src="./public/assets/videos/evadus_core/spoofing_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/spoofing_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/bypass_demo.jpg">
<source src="./public/assets/videos/evadus_core/bypass_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/bypass_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Native Peripheral Support</h4>
<p class="feature-description">No more USB-Passthrough headaches. Since it's the same OS kernel, your webcam, microphone, and mouse work natively. No complex driver setup or "virtual camera" detection risks.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Zero-Latency Performance</h4>
<p class="feature-description">Unlike cloud solutions, RDP V2 runs locally on your hardware. You get 100% native performance with zero input lag, ensuring you can work fast without network delays.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/fullscreen_demo.jpg">
<source src="./public/assets/videos/evadus_core/fullscreen_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/fullscreen_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
<div class="feature-item">
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/vm_hotkey_demo.jpg">
<source src="./public/assets/videos/evadus_core/vm_hotkey_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/vm_hotkey_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
<div class="feature-text-content">
<h4 class="feature-title">Ghost Controls</h4>
<p class="feature-description">Switch between your "Proctor" session and "Cheat" session instantly with undetectable global hotkeys. Your inputs in the hidden session are invisible to keyloggers running in the main session.</p>
</div>
</div>
<div class="feature-item">
<div class="feature-text-content">
<h4 class="feature-title">Seamless File Access</h4>
<p class="feature-description">No need to upload files to a server. Both sessions share your physical hard drive. Your notes, PDFs, and cheat sheets are instantly accessible in the hidden session without moving any files.</p>
</div>
<div class="feature-image-placeholder">
<video autoplay loop muted playsinline loading="lazy" poster="./public/assets/videos/posters/sharing_demo.jpg">
<source src="./public/assets/videos/evadus_core/sharing_demo.webm" type="video/webm">
<source src="./public/assets/videos/evadus_core/sharing_demo.mp4" type="video/mp4">
Your browser does not support the video tag.
</video>
</div>
</div>
</div>
</div>
</div>
</div>
</div>
</section>
<section class="pricing-section-new" id="pricing">
<div class="container">
<h2 class="section-title manifesto-title" data-text="PRICING">PRICING</h2>


<div class="pricing-toggle-container">
<button class="pricing-toggle-btn active" data-period="monthly">Monthly</button>
<button class="pricing-toggle-btn" data-period="annually">Annually</button>
</div>
<div class="pricing-grid-new">
<div class="pricing-card-new">
<div class="pricing-card-header">
<div class="plan-header-top">
<h3 class="plan-title-new">Basic</h3>
<img src="/assets/basic_tier-9be70a09.png" alt="Basic Plan Icon" class="plan-icon-new">
</div>
<p class="plan-price-new">
$0
</p>
</div>
<div class="plan-cta-new">
<button class="plan-button-new free-btn">Download Now</button>
</div>
<p class="plan-description-new">Default account features.</p>
<ul class="plan-features-new">
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: rgb(0, 255, 0);"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg><b>Evadus+ Access</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg><b>AI Chat (BYOK only)</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>Access to 1 AI model (Gemini 2.5 Flash Lite)<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>1 screenshot/audio clip per request<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>5 Requests per day<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>Additional context<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>File Uploads<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: rgb(0, 255, 0);"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Unlimited Evadus Lite Access<span class="feature-tag tag-el">[ EL ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>Screensharing<span class="feature-tag tag-u">[ U ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>Remote Access Tool<span class="feature-tag tag-u">[ U ]</span></li>
</ul>
</div>


<div class="pricing-card-new">
<div class="pricing-card-header">
<div class="plan-header-top">
<h3 class="plan-title-new">Premium</h3>
<img src="/assets/premium_tier-5b38d73c.png" alt="Premium Plan Icon" class="plan-icon-new">
</div>
<p class="plan-price-new">
<span class="price-discount-group">
<span id="standard-price">$35</span>
<span class="price-per-month" id="standard-price-unit">/month</span>
</span>
<span class="price-regular" id="standard-price-regular"></span>
</p>
</div>


<div class="plan-buttons-grid">
<div class="plan-cta-new" id="standard-paypal-cta">
<button class="plan-button-new paypal-btn interactive-paypal" id="standard-paypal-btn">
<img src="/assets/paypal_logo-9b9a8044.png" alt="PayPal" class="payment-logo">
</button>
</div>
<div class="plan-cta-new" id="standard-bitcoin-cta">
<button class="plan-button-new bitcoin-btn interactive-btc" id="standard-bitcoin-btn">
<img src="/assets/bitcoin-1602a8af.png" alt="Bitcoin" class="payment-logo">
</button>
</div>
</div>
<p class="plan-description-new">Everything in Basic, plus...</p>
<ul class="plan-features-new">
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#5ebffd" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg><b>Evadus+ Access</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg><b>AI Chat (Gemini models or BYOK)</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>Access to all Gemini Models (3.1 Pro, 2.5 Pro etc.)<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>5 screenshots/audio clips per request<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#5ebffd" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Unlimited Requests<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>Default additional context presets<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#5ebffd" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Customizable Hotkeys<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd" /></svg>File Uploads<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="currentColor" class="checkmark-icon" style="color: gray;"><path fill-rule="evenodd" d="M3 10a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z" clip-rule="evenodd" /></svg>2 hours of screensharing<span class="feature-tag tag-u">[ U ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#5ebffd" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Remote Access Tool<span class="feature-tag tag-u">[ U ]</span></li>
</ul>
</div>


<div class="pricing-card-new featured">
<div class="pricing-card-header">
<div class="plan-header-top">
<h3 class="plan-title-new">Unlimited</h3>
<img src="/assets/unlimited_tier-9ce491ad.png" alt="Unlimited Plan Icon" class="plan-icon-new">
</div>
<p class="plan-price-new">
<span class="price-discount-group">
<span id="pro-price">$60</span>
<span class="price-per-month" id="pro-price-unit">/month</span>
</span>
<span class="price-regular" id="pro-price-regular"></span>
</p>
</div>


<div class="plan-buttons-grid">
<div class="plan-cta-new" id="pro-paypal-cta">
<button class="plan-button-new paypal-btn interactive-paypal" id="pro-paypal-btn">
<img src="/assets/paypal_logo-9b9a8044.png" alt="PayPal" class="payment-logo">
</button>
</div>
<div class="plan-cta-new" id="pro-bitcoin-cta">
<button class="plan-button-new bitcoin-btn interactive-btc" id="pro-bitcoin-btn">
<img src="/assets/bitcoin-1602a8af.png" alt="Bitcoin" class="payment-logo">
</button>
</div>
</div>
<p class="plan-description-new">Everything in Premium, plus...</p>
<ul class="plan-features-new">
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg><b>Evadus+ Access</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg><b>AI Chat (BYOK & Unlimited access to all LLM's)</b><span class="feature-tag tag-ep">[ E+ ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Access to all AI models (Claude 4.6 Opus, Grok 4, GPT-5.2, Deepseek V3.2 R1)<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Unlimited Screenshots and Audio Clips per request<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Unlimited Requests<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Customizable presets & context<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>File Uploads<span class="feature-tag tag-ec">[ EC ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Unlimited Screensharing<span class="feature-tag tag-u">[ U ]</span></li>
<li><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 20" fill="#ce53ff" class="checkmark-icon"><path fill-rule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clip-rule="evenodd" /></svg>Remote Access Tool<span class="feature-tag tag-u">[ U ]</span></li>
</ul>
</div>
</div>


<div class="pricing-legend">
<span class="legend-item"><span class="feature-tag tag-u">[ U ]</span> Universal</span>
<span class="legend-item"><span class="feature-tag tag-el">[ EL ]</span> Evadus Lite</span>
<span class="legend-item"><span class="feature-tag tag-ec">[ EC ]</span> Evadus Core</span>
<span class="legend-item"><span class="feature-tag tag-ep">[ E+ ]</span> Evadus+</span>
</div>


<div class="terms-checkbox-container" style="display: flex; align-items: center; justify-content: center; gap: 10px; margin-top: 25px; color: #888; font-size: 0.9rem;">
<input type="checkbox" id="terms-checkbox" style="accent-color: #00ff00; width: 16px; height: 16px; cursor: pointer;">
<label for="terms-checkbox" style="cursor: pointer;">
I have read and agree to the <a href="#" id="open-agreement-btn" style="color: #00ff00; text-decoration: underline; transition: color 0.2s;">Unified Terms and Policies</a>.
</label>
</div>


<p id="pricing-auth-warning" style="font-family: Arial, sans-serif; color: #ff4500; text-align: center; margin-top: 20px; font-weight: 500; display: none;"></p>
</div>
</section>
<section class="compatibility-section" id="compatibility">
<div class="container">
<h2 class="section-title manifesto-title" data-text="NO_EXAM_IS_SAFE">NO_EXAM_IS_SAFE</h2>
<div class="compatibility-container">
<div class="compatibility-controls">
<input type="text" id="proctor-search" placeholder="> Search for your proctor...">
</div>
<div class="tabs-nav">
<button class="tab-link" data-tab="lite">[EVADUS LITE]</button>
<button class="tab-link active" data-tab="evadus">[EVADUS CORE]</button>
<button class="tab-link" data-tab="plus">[EVADUS+]</button>
</div>
<div class="tabs-content">
<div id="lite-tab" class="tab-panel">
</div>
<div id="evadus-tab" class="tab-panel active">
</div>
<div id="plus-tab" class="tab-panel">
</div>
</div>
<p id="no-results-message" style="text-align: center; color: #888; margin-top: 30px; display: none;">// NO MATCHING SYSTEMS FOUND //<br>If your proctor isn't listed, we likely haven't tested it yet. <a href="https://discord.gg/64ZsNteakp" class="basic-checks-link">Join our Discord</a> and ask the community!</p>
</div>
</div>
</section>
<section class="leaderboard-section" id="leaderboard">
<div class="container">
<h2 class="section-title manifesto-title" data-text="TOP_USERS">TOP_USERS</h2>
<div class="leaderboard-container">
<ol id="leaderboard-list" class="leaderboard-list">
<li class="leaderboard-loading">// Accessing secure server... Transmitting data...</li>
</ol>
</div>
</div>
</section>


<section class="help-section" id="help">
<div class="container">
<h2 class="section-title manifesto-title" data-text="NEED_HELP?">NEED_HELP?</h2>


<div class="hacker-help-container">
<div class="help-terminal-header">
<h3 class="panel-title" data-text="COMMUNITY_CHANNELS">COMMUNITY_CHANNELS</h3>
</div>
<ul class="community-links-cli">
<li>
<span class="cli-prompt">&gt;</span>
<img src="/assets/discord_logo-5440c07f.png" alt="Discord" class="social-logo">
<a href="https://discord.gg/64ZsNteakp" target="_blank">join discord server</a>
</li>
<li>
<span class="cli-prompt">&gt;</span>
<img src="/assets/reddit_logo-2245d08b.png" alt="Reddit" class="social-logo">
<a href="https://www.reddit.com/r/Evadus/" target="_blank">browse subreddit r/Evadus</a>
</li>
<li>
<span class="cli-prompt">&gt;</span>
<img src="/assets/youtube_logo-c33c965d.png" alt="YouTube" class="social-logo">
<a href="https://www.youtube.com/@evaduscheats" target="_blank">youtube @evaduscheats</a>
</li>
</ul>


<div class="help-terminal-header faq-header">
<h3 class="panel-title" data-text="FAQ_DATABASE">FAQ_DATABASE</h3>
</div>
<div class="faq-list">
<div class="faq-item">
<div class="faq-question">
<h4>How is Evadus Lite and Evadus Core undetectable?</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Both Evadus Lite and Core operates at the system level, creating an invisible overlay that can't be seen by any type of screen capture, or static detection. Our app also uses polymorphism to make sure it can't get blacklisted. Furthermore, Evadus Core allows mouse clicks to pass through, preventing other apps from ever losing focus, Evadus Lite does not have this feature.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>This is unethical!!</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Please contact our PR department at <a href="mailto:signup@grindr.com?subject=Unemployed%20Single%20looking%20for%20some%20action" class="basic-checks-link">signup@grindr.com</a>.<br>We take your concerns very serious.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>What's the difference between Evadus Lite, Evadus Core, and Evadus+?</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p><strong>Evadus Lite</strong> is free and lets you access a web browser on an invisible overlay. <strong>Evadus Core</strong> adds AI-powered answer generation with advanced invisibility features. <strong>Evadus+</strong> is a modified RDP V2 client (currently in Open Beta) bypassing even the strictest lockdown browsers.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>Something isn't working, I NEED HELP ASAP.</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Join the <a href="https://discord.gg/64ZsNteakp" class="basic-checks-link">discord server</a>, staff are online 24/7, and you will likely be helped within a few hours. You can also join the <a href="https://www.reddit.com/r/Evadus/" class="basic-checks-link">reddit</a>, though we are less active there.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>How do I know this is legit?</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Go to our <a href="https://www.youtube.com/@evaduscheats" class="basic-checks-link">Youtube</a> to see live showcases of how we beat different proctors. Our <a href="https://discord.gg/64ZsNteakp" class="basic-checks-link">discord server</a> is also full of users that can vouch for it.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>The features mention "Polymorphism." What is that?</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Polymorphism means that every user gets a unique version of the Evadus application. This is a key security feature that prevents proctoring companies from creating a single signature to detect and blacklist our software. It ensures your version of Evadus remains truly unique and unrecognized.</p>
</div>
</div>
<div class="faq-item">
<div class="faq-question">
<h4>Does this work with {proctor name}?</h4>
<button class="faq-toggle" aria-label="Toggle answer">
<svg viewBox="0 0 24 24" fill="currentColor">
<path d="M7 10l5 5 5-5z"/>
</svg>
</button>
</div>
<div class="faq-answer">
<p>Check the <a href="#compatibility" class="basic-checks-link">compatbiility section</a>. If your proctor name isn't on the list, there's a good chance we just haven't tested it yet or haven't heard about it. However, join our <a href="https://discord.gg/64ZsNteakp" class="basic-checks-link">discord server</a> and ask around, there's a good chance someone has used it and can confirm for you.</p>
</div>
</div>


</div>
</div>
</div>
</section>
<div id="mac-modal" class="modal">
<div class="modal-content hacker-modal-content" style="max-width: 1200px; width: 90%;">
<div class="terminal-header">
<div class="terminal-title">// SYSTEM_ALERT [code: OS_INCOMPATIBLE]</div>
<div class="terminal-buttons">
<span class="t-btn t-red"></span>
<span class="t-btn t-yellow"></span>
<span class="t-btn t-green"></span>
</div>
</div>
<div class="hacker-modal-body">
<span class="close-btn">×</span>
<img src="/assets/apple_logo-64f88d57.png" alt="Apple" class="hacker-modal-logo">
<h3 class="panel-title" data-text="[MacOS] - STATUS_UPDATE">[MacOS] - STATUS_UPDATE</h3>


<p class="hacker-modal-text">
Since transitioning our architecture from a QEMU VM to an advanced native Windows RDP session, MacOS support is fundamentally impossible. To run Evadus, you will need a Windows machine.
</p>


<p class="hacker-modal-text">
<span class="cli-prompt">&gt;</span> <b>HARDWARE GUIDELINES (BUDGET FRIENDLY):</b><br>
Evadus is incredibly lightweight and optimized. You do not need a high-end setup; these are just suggestions, not hard requirements. If you have better hardware, feel free to use it.<br><br>
<span class="cli-prompt">~</span> <b>OS:</b> Windows 11 (Preferred) or newer Windows 10 builds.<br>
<span class="cli-prompt">~</span> <b>CPU:</b> Average i3, i5, or Ryzen (lower-spec is perfectly fine).<br>
<span class="cli-prompt">~</span> <b>RAM:</b> 8GB is more than enough.<br>
<span class="cli-prompt">~</span> <b>GPU:</b> No dedicated GPU required (an iGPU works flawlessly).<br>
<span class="cli-prompt">~</span> <b>Storage:</b> Minimal. RDP is built-in and our app is ~300MB.
</p>


<p class="hacker-modal-text">
<span class="cli-prompt">&gt;</span> <b>WHERE TO GET ONE:</b><br>
The second-hand market (eBay, Facebook Marketplace) has plenty of cheap options that exceed these specs. If you prefer to buy new or refurbished from Amazon, check out these generic searches:<br><br>
<a href="https://www.amazon.com/s?k=budget+windows+11+laptop+8gb+ram" target="_blank" class="cli-link" style="display: block; margin-bottom: 8px;">> [AMAZON] Budget Windows 11 Laptops</a>
<span class="cli-comment" style="font-size: 0.85rem; color: #ffbd2e; display: block; line-height: 1.4;">// DISCLAIMER: These are strictly non-affiliate links. We make $0 if you click or buy from them. They are provided purely for your convenience.</span>
</p>


<div class="hacker-modal-buttons">
<button class="hacker-modal-btn cancel-btn" onclick="document.getElementById('mac-modal').style.display='none'">[ACKNOWLEDGE]</button>
</div>
</div>
</div>
</div>
<div id="loading-overlay" class="loading-overlay">
<div class="spinner-container">
<div class="spinner"></div>
<p class="spinner-text">// TIME_TO_CHEAT...</p>
<div class="progress-bar-container">
<div id="progress-bar" class="progress-bar"></div>
</div>
<button id="cancel-payment-btn" class="hacker-modal-btn cancel-btn">[CANCEL]</button>
</div>
</div>
<div id="agreement-modal" class="modal">
<div class="modal-content hacker-modal-content" style="max-width: 800px; width: 90%;">
<div class="terminal-header">
<div class="terminal-title">// SYSTEM_DOCUMENT [UNIFIED_TERMS_AND_POLICIES.md]</div>
<div class="terminal-buttons">
<span class="t-btn t-red"></span>
<span class="t-btn t-yellow"></span>
<span class="t-btn t-green"></span>
</div>
</div>
<div class="hacker-modal-body">
<span class="close-btn agreement-close-btn">×</span>
<h3 class="panel-title" data-text="UNIFIED_TERMS_AND_POLICIES">UNIFIED_TERMS_AND_POLICIES</h3>


<div class="terminal-body" style="max-height: 60vh; overflow-y: auto; text-align: left; margin-top: 15px; padding: 15px; background: rgba(0,0,0,0.6); border: 1px solid #333; border-radius: 4px;">
<div id="agreement-content" style="font-family: monospace; color: #ccc; font-size: 0.85rem; line-height: 1.5;">
<span class="cli-prompt">&gt;</span> FETCHING_DOCUMENT...
</div>
</div>


<div class="hacker-modal-buttons" style="margin-top: 20px;">
<button class="hacker-modal-btn cancel-btn">[CLOSE]</button>
</div>
</div>
</div>
</div>
<script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/tsparticles/2.12.0/tsparticles.bundle.min.js"></script>


</body>


</html>

