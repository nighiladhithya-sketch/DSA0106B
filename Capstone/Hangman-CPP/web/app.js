/* Hangman — C++ Edition : browser client
   All game rules (words, guesses, lives, hints, timer, scoring, leaderboard) live in the C++ engine.
   This file only sends the player's actions to the C++ server and animates the replies. */
'use strict';

const $ = (s) => document.querySelector(s);
const $$ = (s) => Array.from(document.querySelectorAll(s));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const TOTAL_STEPS = 10;            // platform, pole, beam, rope, head, body, 2 arms, 2 legs
const ROWS = ['QWERTYUIOP', 'ASDFGHJKL', 'ZXCVBNM'];

const store = {
  get(k, d) { try { const v = localStorage.getItem(k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
  set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) { /* storage unavailable */ } },
};

let config = null;
let state = null;
let busy = false;
let shownSteps = 0;
let timer = null;
let lastTickSecond = null;
let resultTimeout = null;
const prefs = {
  name: store.get('hm.name', ''),
  difficulty: store.get('hm.difficulty', 'Medium'),
  category: store.get('hm.category', 'Random'),
  length: store.get('hm.length', 'any'),
  mode: 'solo',
};

/* =============================================================== sound (Web Audio, no files) */
const Sound = {
  ctx: null,
  on: store.get('hm.sound', true),
  ensure() {
    if (!this.ctx) { const C = window.AudioContext || window.webkitAudioContext; if (C) this.ctx = new C(); }
    if (this.ctx && this.ctx.state === 'suspended') this.ctx.resume();
    return this.ctx;
  },
  tone(freq, dur, { type = 'sine', vol = 0.18, slide = null, delay = 0 } = {}) {
    const c = this.ctx, t = c.currentTime + delay;
    const o = c.createOscillator(), g = c.createGain();
    o.type = type; o.frequency.setValueAtTime(freq, t);
    if (slide) o.frequency.exponentialRampToValueAtTime(slide, t + dur);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(vol, t + 0.015);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    o.connect(g).connect(c.destination); o.start(t); o.stop(t + dur + 0.05);
  },
  noise(dur, { vol = 0.25, freq = 800, q = 1, type = 'bandpass', delay = 0, sweep = null } = {}) {
    const c = this.ctx, t = c.currentTime + delay;
    const buf = c.createBuffer(1, Math.ceil(c.sampleRate * dur), c.sampleRate);
    const data = buf.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
    const src = c.createBufferSource(), f = c.createBiquadFilter(), g = c.createGain();
    src.buffer = buf; f.type = type; f.frequency.setValueAtTime(freq, t); f.Q.value = q;
    if (sweep) f.frequency.exponentialRampToValueAtTime(sweep, t + dur);
    g.gain.setValueAtTime(vol, t); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    src.connect(f).connect(g).connect(c.destination); src.start(t);
  },
  play(name) {
    if (!this.on || !this.ensure()) return;
    switch (name) {
      case 'click': this.tone(520, 0.05, { type: 'triangle', vol: 0.06 }); break;
      case 'correct': this.tone(660, 0.12, { vol: 0.16 }); this.tone(990, 0.18, { vol: 0.14, delay: 0.08 }); break;
      case 'wrong': this.tone(150, 0.32, { type: 'sawtooth', vol: 0.12, slide: 70 }); this.noise(0.18, { vol: 0.18, freq: 300 }); break;
      case 'build': this.noise(0.07, { vol: 0.12, freq: 1400, q: 4 }); this.tone(220, 0.06, { type: 'square', vol: 0.03 }); break;
      case 'hint': [880, 1175, 1568].forEach((f, i) => this.tone(f, 0.16, { type: 'triangle', vol: 0.1, delay: i * 0.07 })); break;
      case 'error': this.tone(300, 0.1, { type: 'square', vol: 0.05 }); this.tone(240, 0.12, { type: 'square', vol: 0.05, delay: 0.09 }); break;
      case 'tick': this.tone(1500, 0.03, { type: 'square', vol: 0.04 }); break;
      case 'thunder': this.noise(1.6, { vol: 0.35, freq: 400, type: 'lowpass', sweep: 60 }); break;
      case 'win': [523, 659, 784, 1047, 1319].forEach((f, i) => this.tone(f, 0.28, { type: 'triangle', vol: 0.13, delay: i * 0.1 })); break;
      case 'lose':
        this.noise(0.5, { vol: 0.2, freq: 900, q: 8, sweep: 300 });                       // trapdoor creak
        this.tone(90, 0.5, { type: 'sine', vol: 0.4, slide: 40, delay: 0.42 });            // thud
        [392, 370, 311, 262].forEach((f, i) => this.tone(f, 0.45, { type: 'sawtooth', vol: 0.05, delay: 0.7 + i * 0.28 }));
        break;
      default: break;
    }
  },
};

/* =============================================================== server API */
async function api(path, params = {}) {
  const query = new URLSearchParams(params).toString();
  const res = await fetch(`/api/${path}${query ? `?${query}` : ''}`, { cache: 'no-store' });
  return res.json();
}

/* =============================================================== helpers */
function toast(text, kind = 'warn') {
  const el = document.createElement('div');
  el.className = `toast ${kind}`;
  el.textContent = text;
  $('#toasts').appendChild(el);
  setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 320); }, 2400);
}

function replay(el, cls) { el.classList.remove(cls); void el.getBoundingClientRect(); el.classList.add(cls); }

function floater(text, kind, anchor) {
  const wrap = $('#sceneWrap');
  const el = document.createElement('div');
  el.className = `floater ${kind}`;
  el.textContent = text;
  const r = (anchor || wrap).getBoundingClientRect(), w = wrap.getBoundingClientRect();
  el.style.left = `${r.left - w.left + r.width / 2}px`;
  el.style.top = `${r.top - w.top + (anchor ? -8 : r.height * 0.3)}px`;
  wrap.appendChild(el);
  setTimeout(() => el.remove(), 1200);
}

function caption(text, kind = '') {
  const el = $('#sceneCaption');
  el.textContent = text;
  el.className = `scene-caption ${kind}`;
}

function openModal(id) { $$('.modal.open').forEach((m) => m.classList.remove('open')); $(id).classList.add('open'); }
function closeModals() { $$('.modal.open').forEach((m) => m.classList.remove('open')); }
const modalOpen = () => !!$('.modal.open');

/* =============================================================== background */
function buildStars() {
  const box = $('#stars');
  for (let i = 0; i < 140; i++) {
    const s = document.createElement('span');
    const size = Math.random() < 0.85 ? 1 + Math.random() * 1.5 : 2.5 + Math.random();
    Object.assign(s.style, {
      left: `${Math.random() * 100}%`, top: `${Math.random() * 75}%`, width: `${size}px`, height: `${size}px`,
    });
    s.style.setProperty('--d', `${2 + Math.random() * 4}s`);
    s.style.setProperty('--delay', `${-Math.random() * 6}s`);
    box.appendChild(s);
  }
}

const Confetti = {
  run(duration = 3600) {
    const cv = $('#confetti'), ctx = cv.getContext('2d');
    cv.width = innerWidth; cv.height = innerHeight;
    const colors = ['#ffb547', '#3ddc97', '#7cc6ff', '#ff4d6d', '#f4efe6', '#c39bff'];
    const parts = Array.from({ length: 180 }, () => ({
      x: cv.width / 2 + (Math.random() - 0.5) * cv.width * 0.3, y: cv.height * 0.35,
      vx: (Math.random() - 0.5) * 14, vy: -6 - Math.random() * 12, s: 5 + Math.random() * 6,
      r: Math.random() * Math.PI, vr: (Math.random() - 0.5) * 0.3, c: colors[(Math.random() * colors.length) | 0],
    }));
    const start = performance.now();
    const frame = (now) => {
      const t = now - start;
      ctx.clearRect(0, 0, cv.width, cv.height);
      parts.forEach((p) => {
        p.vy += 0.32; p.vx *= 0.99; p.x += p.vx; p.y += p.vy; p.r += p.vr;
        ctx.save(); ctx.translate(p.x, p.y); ctx.rotate(p.r);
        ctx.globalAlpha = Math.max(0, 1 - t / duration);
        ctx.fillStyle = p.c; ctx.fillRect(-p.s / 2, -p.s / 4, p.s, p.s / 2); ctx.restore();
      });
      if (t < duration) requestAnimationFrame(frame); else ctx.clearRect(0, 0, cv.width, cv.height);
    };
    requestAnimationFrame(frame);
  },
};

/* =============================================================== scene */
const scene = () => $('#scene');

function setStep(i, on) {
  $$(`#scene [data-step="${i}"]`).forEach((el) => el.classList.toggle('on', on));
  if (i === 4) scene().classList.toggle('has-head', on);
}

function resetScene() {
  const sc = scene();
  sc.classList.remove('won', 'lost', 'has-head');
  for (let i = 0; i < TOTAL_STEPS; i++) setStep(i, false);
  shownSteps = 0;
  setMood('calm');
  document.body.classList.remove('danger');
  sc.style.setProperty('--sway', '0deg');
}

async function drawSteps(target, delay = 0, withSound = true) {
  while (shownSteps < target) {
    const i = shownSteps++;
    setStep(i, true);
    const el = $(`#scene [data-step="${i}"]`);
    if (el && el.classList.contains('stroke')) { el.classList.add('fresh'); setTimeout(() => el.classList.remove('fresh'), 900); }
    if (withSound) Sound.play('build');
    if (delay) await sleep(delay);
  }
}

function setMood(mood) {
  const sc = scene();
  sc.classList.remove('mood-calm', 'mood-ok', 'mood-worried', 'mood-scared', 'mood-dead', 'mood-happy');
  sc.classList.add(`mood-${mood}`);
}

function moodFor(d) {
  if (d.status === 'won') return 'happy';
  if (d.status === 'lost') return 'dead';
  const ratio = d.livesLeft / d.maxLives;
  if (d.livesLeft <= 1) return 'scared';
  if (ratio <= 0.5) return 'worried';
  if (ratio < 1) return 'ok';
  return 'calm';
}

function updateAtmosphere(d) {
  const playing = d.status === 'playing';
  const danger = playing && d.livesLeft <= 2;
  document.body.classList.toggle('danger', danger);
  const sc = scene();
  sc.style.setProperty('--sway', `${playing ? Math.min(d.livesLost * 0.9, 6) : 0}deg`);
  sc.style.setProperty('--sway-speed', danger ? '1.6s' : '3.2s');
  setMood(moodFor(d));
}

/* =============================================================== panel rendering */
function buildKeyboard() {
  const kb = $('#keyboard');
  kb.innerHTML = '';
  ROWS.forEach((row) => {
    const r = document.createElement('div');
    r.className = 'kb-row';
    [...row].forEach((ch) => {
      const b = document.createElement('button');
      b.className = 'key'; b.textContent = ch; b.dataset.letter = ch; b.type = 'button';
      b.setAttribute('aria-label', `Guess ${ch}`);
      b.addEventListener('click', () => guess(ch));
      r.appendChild(b);
    });
    kb.appendChild(r);
  });
}

function renderKeyboard(d, justLetter) {
  const good = new Set(d.correctLetters), bad = new Set(d.wrongLetters), over = d.status !== 'playing';
  $$('.key').forEach((k) => {
    const L = k.dataset.letter;
    k.classList.toggle('good', good.has(L));
    k.classList.toggle('bad', bad.has(L));
    k.disabled = over || good.has(L) || bad.has(L);
    if (L === justLetter) replay(k, 'just');
  });
}

function renderLives(d, animateFrom) {
  const box = $('#lives');
  if (box.children.length !== d.maxLives) {
    box.innerHTML = '';
    for (let i = 0; i < d.maxLives; i++) {
      const h = document.createElement('span');
      h.className = 'heart';
      h.innerHTML = '<svg><use href="#i-heart"/></svg>';
      box.appendChild(h);
    }
  }
  [...box.children].forEach((h, i) => {
    const alive = i < d.livesLeft;
    if (!alive && animateFrom !== undefined && i >= d.livesLeft && i < animateFrom) {
      h.classList.remove('empty'); replay(h, 'breaking');
      setTimeout(() => { h.classList.remove('breaking'); h.classList.add('empty'); }, 760);
    } else if (!h.classList.contains('breaking')) {
      h.classList.toggle('empty', !alive);
    }
  });
}

function buildWord(d) {
  const box = $('#word');
  box.innerHTML = '';
  box.classList.remove('won');
  let group = null, idx = 0;
  d.masked.forEach((ch, i) => {
    if (ch === ' ') { group = null; return; }
    if (!group) { group = document.createElement('div'); group.className = 'word-group'; box.appendChild(group); }
    const t = document.createElement('div');
    t.className = 'tile'; t.dataset.index = i; t.style.setProperty('--i', idx++);
    t.innerHTML = '<div class="tile-inner"></div>';
    if (ch !== '_') { t.classList.add('revealed'); t.firstChild.textContent = ch; }
    group.appendChild(t);
  });
}

function sparkle(tile) {
  for (let i = 0; i < 8; i++) {
    const s = document.createElement('span');
    s.className = 'spark';
    const a = (Math.PI * 2 * i) / 8;
    s.style.setProperty('--dx', `${Math.cos(a) * 34}px`);
    s.style.setProperty('--dy', `${Math.sin(a) * 34}px`);
    tile.appendChild(s);
    setTimeout(() => s.remove(), 750);
  }
}

function updateWord(prev, d, hinted) {
  let n = 0;
  d.masked.forEach((ch, i) => {
    if (ch === ' ' || ch === '_' || (prev && prev.masked[i] === ch)) return;
    const tile = $(`.tile[data-index="${i}"]`);
    if (!tile) return;
    setTimeout(() => {
      tile.firstChild.textContent = ch;
      tile.classList.add('revealed');
      if (hinted) tile.classList.add('hinted');
      replay(tile, 'flip');
      sparkle(tile);
    }, n++ * 90);
  });
  return n;
}

function revealMissed(d) {
  [...d.word].forEach((ch, i) => {
    if (ch === ' ' || d.masked[i] !== '_') return;
    const tile = $(`.tile[data-index="${i}"]`);
    if (!tile) return;
    setTimeout(() => { tile.firstChild.textContent = ch; tile.classList.add('missed'); replay(tile, 'flip'); }, 300 + i * 70);
  });
}

function renderClue(d, animate) {
  const box = $('#clue'), text = $('#clueText');
  if (d.clue) {
    text.textContent = d.clue;
    box.classList.remove('locked');
    if (animate) replay(box, 'reveal');
  } else {
    text.textContent = d.hasClue ? 'Clue hidden — use a hint to reveal it' : 'No clue for this word';
    box.classList.add('locked');
  }
}

function renderControls(d) {
  const playing = d.status === 'playing';
  $('#hintCount').textContent = d.hintsLeft;
  $('#btnHintLetter').disabled = !playing || d.hintsLeft <= 0;
  $('#btnHintClue').disabled = !playing || !d.hasClue || !!d.clue || d.hintsLeft <= 0;
  $('#solveInput').disabled = !playing;
  $('.btn-solve').disabled = !playing;
  $('#btnGiveUp').disabled = !playing;
  $('#btnNew span').textContent = playing ? 'Skip word' : 'Next word';
  $('#wrongWords').innerHTML = d.wrongWords.map((w) => `<span>${w}</span>`).join('');
  $('#chipCategory').textContent = d.category;
  const lv = $('#chipLevel');
  lv.textContent = d.difficulty;
  lv.className = `chip chip-level lv-${d.difficulty}`;
  $('#footInfo').textContent = `Rounds served by the C++ engine this session: ${d.gamesCreated}`;
}

function renderStats(p, bumpScore) {
  if (!p) return;
  $('#statScore').textContent = p.totalScore;
  $('#statStreak').textContent = p.streak;
  $('#statWins').textContent = p.wins;
  $('#statLosses').textContent = p.losses;
  $('#streakBox').classList.toggle('hot', p.streak >= 2);
  if (bumpScore) replay($('#statScore').parentElement, 'bump');
}

/* =============================================================== timer (Hard mode) */
function startTimer(d) {
  stopTimer();
  const box = $('#timer');
  if (!d.timeLimit || d.status !== 'playing') { box.classList.add('hidden'); return; }
  box.classList.remove('hidden');
  const base = { left: d.secondsLeft, at: performance.now(), total: d.timeLimit };
  const tick = async () => {
    const left = Math.max(0, base.left - (performance.now() - base.at) / 1000);
    const secs = Math.ceil(left);
    $('#timerText').textContent = secs;
    $('#timerBar').style.strokeDashoffset = `${100 - (left / base.total) * 100}`;
    box.classList.toggle('urgent', left <= 10);
    if (left <= 10 && secs !== lastTickSecond && secs > 0) { lastTickSecond = secs; Sound.play('tick'); }
    if (left <= 0) {
      stopTimer();
      onState(await api('state'));       // the C++ engine decides the round is lost on time
    }
  };
  tick();
  timer = setInterval(tick, 200);
}
function stopTimer() { if (timer) clearInterval(timer); timer = null; }

/* =============================================================== state handling */
function onState(d) {
  if (d.event === 'error' && d.error) { toast(d.error); Sound.play('error'); }
  renderStats(d.player, d.awarded > 0);
  if (!d.hasGame) return;

  const prev = state;
  state = d;
  if (d.event === 'new' || !prev) return newRound(d);

  const livesLostNow = d.livesLost - prev.livesLost;
  const hinted = d.event === 'hint';
  const revealed = updateWord(prev, d, hinted);

  renderKeyboard(d, d.letter);
  renderClue(d, d.event === 'clue');
  renderControls(d);
  renderLives(d, livesLostNow > 0 ? prev.livesLeft : undefined);

  if (d.event === 'correct' || d.event === 'solved') {
    Sound.play('correct');
    if (revealed) floater(`+${revealed}`, 'good', $('#word'));
    caption(revealed > 1 ? `${revealed} letters revealed!` : 'Nice guess!', 'good');
  } else if (hinted) {
    Sound.play('hint');
    caption(`Hint: the word contains ${d.letter}`, 'warn');
  } else if (d.event === 'clue') {
    Sound.play('hint');
    caption('Clue revealed', 'warn');
  }

  if (livesLostNow > 0) {
    Sound.play('wrong');
    replay($('#sceneWrap'), 'shake');
    replay($('#vignette'), 'go');
    floater(`-${livesLostNow} ${livesLostNow > 1 ? 'lives' : 'life'}`, 'bad');
    const steps = TOTAL_STEPS - d.maxLives + d.livesLost;
    drawSteps(Math.min(steps, TOTAL_STEPS), 320, false);
    if (d.livesLeft <= 2 && d.status === 'playing') {
      setTimeout(() => { replay($('#flash'), 'go'); Sound.play('thunder'); }, 250);
    }
    const left = d.livesLeft;
    if (d.status === 'playing') {
      caption(left === 1 ? 'Last chance!' : d.event === 'wrongWord' ? 'Wrong word — a piece is added!' : `${left} lives left`,
        left <= 2 ? 'bad' : 'warn');
    }
  }

  updateAtmosphere(d);
  if (d.status !== 'playing' && prev.status === 'playing') endRound(d);
  else if (d.status === 'playing') startTimer(d);
}

async function newRound(d) {
  clearTimeout(resultTimeout);
  closeModals();
  resetScene();
  buildWord(d);
  renderKeyboard(d);
  renderLives(d);
  renderClue(d, false);
  renderControls(d);
  $('#solveInput').value = '';
  caption(d.custom ? 'Two-player round — good luck!' : `Category: ${d.category}`);
  startTimer(d);
  await sleep(150);
  await drawSteps(TOTAL_STEPS - d.maxLives + d.livesLost, 170, true);   // gallows builds itself
  updateAtmosphere(d);
}

async function endRound(d) {
  stopTimer();
  renderKeyboard(d);
  const sc = scene();
  if (d.status === 'won') {
    for (let i = 4; i < TOTAL_STEPS; i++) setStep(i, true);   // he is safe: draw him whole
    shownSteps = Math.max(shownSteps, TOTAL_STEPS);
    setMood('happy');
    sc.classList.add('won');
    $('#word').classList.add('won');
    caption('You saved him!', 'good');
    Sound.play('win');
    Confetti.run();
  } else {
    await drawSteps(TOTAL_STEPS, 120, false);
    await sleep(350);
    setMood('dead');
    sc.classList.add('lost');
    revealMissed(d);
    caption(d.endReason === 'time up' ? 'Time is up…' : d.endReason === 'gave up' ? 'You gave up…' : 'Hanged…', 'bad');
    Sound.play('lose');
  }
  resultTimeout = setTimeout(() => showResult(d), d.status === 'won' ? 1700 : 2600);
}

function showResult(d) {
  const win = d.status === 'won';
  const card = $('.card-result');
  card.classList.toggle('win', win);
  card.classList.toggle('lose', !win);
  $('#resultBadge').innerHTML = win
    ? '<svg viewBox="0 0 24 24"><path d="M5 12.5l4.5 4.5L19 7.5" fill="none" stroke="currentColor" stroke-width="2.8" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    : '<svg viewBox="0 0 24 24"><path d="M6 6l12 12M18 6L6 18" fill="none" stroke="currentColor" stroke-width="2.8" stroke-linecap="round"/></svg>';
  $('#resultTitle').textContent = win ? 'Saved!' : 'Hanged!';
  const reasons = { 'time up': 'The clock ran out.', 'gave up': 'You gave up on this one.', hanged: 'The trapdoor opened.' };
  $('#resultSub').textContent = win ? (d.livesLost === 0 ? 'Flawless — not a single mistake!' : 'You guessed the word in time.') : reasons[d.endReason] || '';
  $('#resultWord').innerHTML = [...d.word].map((ch, i) =>
    ch === ' ' ? '<span class="gap"></span>' : `<span class="${d.masked[i] === '_' ? 'missed' : ''}">${ch}</span>`).join('');
  $('#resultClue').textContent = d.clue || '';
  const m = Math.floor(d.elapsed / 60), s = String(d.elapsed % 60).padStart(2, '0');
  const cells = [
    ['Lives left', d.livesLeft], ['Hints used', d.hintsUsed], ['Time', `${m}:${s}`],
    win ? ['Streak', d.player.streak] : ['Best streak', d.player.bestStreak],
  ];
  $('#resultGrid').innerHTML = cells.map(([k, v]) => `<div><b>${v}</b><small>${k}</small></div>`).join('');
  const bonus = d.awarded - d.roundScore;
  $('#resultRank').textContent = win
    ? (d.rank ? `New leaderboard entry — rank #${d.rank}!` : '') + (bonus > 0 ? `  (+${bonus} streak bonus)` : '')
    : '';
  openModal('#resultModal');
  if (win) countUp($('#resultPoints'), d.awarded);
  setTimeout(() => $('#btnNext').focus(), 350);
}

function countUp(el, target) {
  const start = performance.now(), dur = 900;
  const step = (now) => {
    const t = Math.min(1, (now - start) / dur);
    el.textContent = Math.round(target * (1 - Math.pow(1 - t, 3)));
    if (t < 1) requestAnimationFrame(step);
  };
  requestAnimationFrame(step);
}

/* =============================================================== player actions */
async function act(path, params) {
  if (busy) return null;
  busy = true;
  try { const d = await api(path, params); onState(d); return d; }
  catch (e) { toast('Cannot reach the C++ server. Is hangman still running?'); return null; }
  finally { busy = false; }
}

function guess(letter) {
  if (!state || state.status !== 'playing' || modalOpen()) return;
  const key = $(`.key[data-letter="${letter}"]`);
  if (key && key.disabled) { toast(`You already tried ${letter}`); Sound.play('error'); return; }
  if (key) { key.classList.add('pressed'); setTimeout(() => key.classList.remove('pressed'), 120); }
  Sound.play('click');
  act('guess', { letter });
}

function hasProgress() {
  return state && state.status === 'playing' &&
    (state.livesLost > 0 || state.hintsUsed > 0 || state.masked.some((c, i) => c !== '_' && c !== ' ' && c !== undefined));
}

async function startSolo() {
  prefs.name = $('#nameInput').value.trim() || 'Player';
  store.set('hm.name', prefs.name);
  store.set('hm.difficulty', prefs.difficulty);
  store.set('hm.category', prefs.category);
  store.set('hm.length', prefs.length);
  Sound.ensure();
  if (hasProgress()) await api('forfeit');
  const d = await act('new', { difficulty: prefs.difficulty, category: prefs.category, length: prefs.length, name: prefs.name });
  if (d && d.event === 'error') $('#startError').textContent = d.error;
}

async function startDuo() {
  const word = $('#secretInput').value.trim();
  if (!word) { $('#startError').textContent = 'Player 1: type a secret word first.'; return; }
  prefs.name = $('#nameInput').value.trim() || 'Player';
  store.set('hm.name', prefs.name);
  Sound.ensure();
  if (hasProgress()) await api('forfeit');
  const d = await act('custom', { word, clue: $('#secretClue').value.trim(), difficulty: prefs.difficulty, name: prefs.name });
  if (d && d.event === 'error') { $('#startError').textContent = d.error; openModal('#startModal'); return; }
  $('#secretInput').value = ''; $('#secretClue').value = '';
}

async function nextWord() {
  if (state && state.custom) { openStart('duo'); return; }
  if (state && state.status === 'playing') await act('forfeit');
  act('new', { difficulty: prefs.difficulty, category: prefs.category, length: prefs.length, name: prefs.name || 'Player' });
}

let giveUpArmed = null;
function giveUp() {
  const b = $('#btnGiveUp');
  if (!giveUpArmed) {
    b.classList.add('confirm'); $('#btnGiveUp span').textContent = 'Sure? Click again';
    giveUpArmed = setTimeout(() => { giveUpArmed = null; b.classList.remove('confirm'); $('#btnGiveUp span').textContent = 'Give up'; }, 3000);
    return;
  }
  clearTimeout(giveUpArmed); giveUpArmed = null;
  b.classList.remove('confirm'); $('#btnGiveUp span').textContent = 'Give up';
  act('forfeit');
}

/* =============================================================== start dialog */
function buildStartDialog() {
  $('#nameInput').value = prefs.name;
  const lv = $('#levels');
  lv.innerHTML = config.difficulties.map((d) => `
    <button type="button" class="level" data-level="${d.name}">
      <b>${d.name}</b>
      <small>${d.lives} wrong guesses · ${d.hints} hint${d.hints === 1 ? '' : 's'}${d.timeLimit ? ` · ${d.timeLimit}s timer` : ''}</small>
      <span class="mult">×${d.multiplier.toFixed(1)} points</span>
    </button>`).join('');
  $$('.level').forEach((b) => b.addEventListener('click', () => { prefs.difficulty = b.dataset.level; markChoices(); Sound.play('click'); }));

  const cats = ['Random', ...config.categories];
  $('#cats').innerHTML = cats.map((c) => `<button type="button" class="cat" data-cat="${c}">${c}</button>`).join('');
  $$('.cat').forEach((b) => b.addEventListener('click', () => { prefs.category = b.dataset.cat; markChoices(); Sound.play('click'); }));
  if (!cats.includes(prefs.category)) prefs.category = 'Random';

  const lengths = ['any', '3', '4', '5', '6', '7', '8', '9'];
  $('#lengths').innerHTML = lengths.map((l) => `
    <button type="button" class="cat len" data-len="${l}">${l === 'any' ? 'Any' : l === '9' ? '9+' : l}<small></small></button>`).join('');
  $$('.len').forEach((b) => b.addEventListener('click', () => { prefs.length = b.dataset.len; markChoices(); Sound.play('click'); }));
  if (!lengths.includes(prefs.length)) prefs.length = 'any';
  markChoices();
}

function markChoices() {
  $$('.level').forEach((b) => b.classList.toggle('active', b.dataset.level === prefs.difficulty));
  $$('.cat[data-cat]').forEach((b) => b.classList.toggle('active', b.dataset.cat === prefs.category));
  // Word-length chips show how many words the chosen category has of each length
  const counts = config.lengthCounts[prefs.category] || {};
  $$('.len').forEach((b) => {
    const n = b.dataset.len === 'any' ? Object.values(counts).reduce((a, c) => a + c, 0) : counts[b.dataset.len] || 0;
    b.disabled = n === 0;
    b.querySelector('small').textContent = n;
    b.title = `${n} word${n === 1 ? '' : 's'}`;
  });
  if ($(`.len[data-len="${prefs.length}"]`)?.disabled) prefs.length = 'any';
  $$('.len').forEach((b) => b.classList.toggle('active', b.dataset.len === prefs.length));
}

function setTab(tab) {
  prefs.mode = tab;
  $$('.tab').forEach((t) => t.classList.toggle('active', t.dataset.tab === tab));
  $$('.tab-body').forEach((b) => b.classList.toggle('hidden', b.dataset.body !== tab));
  $('#startError').textContent = '';
}

function openStart(tab = 'solo') {
  setTab(tab);
  $('#startError').textContent = '';
  openModal('#startModal');
  setTimeout(() => (tab === 'duo' ? $('#secretInput') : $('#nameInput')).focus(), 300);
}

/* =============================================================== leaderboard */
async function showBoard() {
  openModal('#boardModal');
  const rows = await api('leaderboard');
  $('#board').innerHTML = rows.length
    ? `<table><thead><tr><th>#</th><th>Name</th><th>Score</th><th>Level</th><th>Word</th><th>Date</th></tr></thead><tbody>${
      rows.map((r, i) => `<tr><td><span class="medal">${i + 1}</span></td><td>${escapeHtml(r.name)}</td><td class="score">${r.score}</td>
        <td>${r.difficulty}</td><td>${r.word}</td><td class="muted">${r.date}</td></tr>`).join('')}</tbody></table>`
    : '<p class="empty">No survivors yet. Save someone to get on the board!</p>';
}

function escapeHtml(s) { return s.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c])); }

/* =============================================================== wiring */
function setSoundIcon() { $('#btnSound use').setAttribute('href', Sound.on ? '#i-sound' : '#i-mute'); }

function bindEvents() {
  $('#btnStart').addEventListener('click', startSolo);
  $('#btnStartDuo').addEventListener('click', startDuo);
  $$('.tab').forEach((t) => t.addEventListener('click', () => setTab(t.dataset.tab)));
  $('#btnPeek').addEventListener('click', () => {
    const i = $('#secretInput'); i.type = i.type === 'password' ? 'text' : 'password';
  });
  $('#btnHintLetter').addEventListener('click', () => act('hint', { type: 'letter' }));
  $('#btnHintClue').addEventListener('click', () => act('hint', { type: 'clue' }));
  $('#solveForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const word = $('#solveInput').value.trim();
    if (!word || !state || state.status !== 'playing') return;
    $('#solveInput').value = '';
    act('solve', { word });
  });
  $('#btnNew').addEventListener('click', nextWord);
  $('#btnGiveUp').addEventListener('click', giveUp);
  $('#btnSettings').addEventListener('click', () => openStart('solo'));
  $('#btnNext').addEventListener('click', nextWord);
  $('#btnResultMode').addEventListener('click', () => openStart(state && state.custom ? 'duo' : 'solo'));
  $('#btnBoard').addEventListener('click', showBoard);
  $('#btnHelp').addEventListener('click', () => openModal('#helpModal'));
  $('#btnSound').addEventListener('click', () => { Sound.on = !Sound.on; store.set('hm.sound', Sound.on); setSoundIcon(); Sound.play('click'); });
  $$('[data-close]').forEach((b) => b.addEventListener('click', closeModals));
  $$('.modal').forEach((m) => m.addEventListener('click', (e) => {
    if (e.target === m && m.id !== 'startModal') closeModals();
  }));

  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      if ($('#startModal').classList.contains('open') && !state) return;
      closeModals(); return;
    }
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    const tag = (e.target.tagName || '').toLowerCase();
    if (tag === 'input') {
      if (e.key === 'Enter' && $('#startModal').classList.contains('open')) {
        (prefs.mode === 'duo' ? startDuo : startSolo)();
      }
      return;
    }
    if (e.key === 'Enter' && $('#resultModal').classList.contains('open')) { e.preventDefault(); nextWord(); return; }
    if (/^[a-z]$/i.test(e.key)) guess(e.key.toUpperCase());
  });
}

async function init() {
  buildStars();
  buildKeyboard();
  bindEvents();
  setSoundIcon();
  try {
    config = await api('config');
  } catch (e) {
    caption('Start the C++ server (hangman.exe) and reload this page', 'bad');
    toast('Cannot reach the C++ game server.');
    return;
  }
  buildStartDialog();
  renderStats(config.player);
  $('#footInfo').textContent = `${config.wordCount} words in ${config.categories.length} categories loaded by the C++ engine`;

  const current = await api('state');               // resume a round after a page reload
  if (current.hasGame && current.status === 'playing') { state = null; onState(Object.assign(current, { event: 'new' })); }
  else openStart('solo');
}

init();
