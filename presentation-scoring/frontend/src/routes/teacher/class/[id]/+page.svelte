<script>
  import { onMount, onDestroy } from 'svelte';
  import { page } from '$app/stores';
  import { auth } from '$lib/stores';
  import { api } from '$lib/api';
  import { goto } from '$app/navigation';

  const classId = $page.params.id;

  let classData = null;
  let qrData = null;
  let activePresentation = null;
  let loading = true;
  let error = '';
  let selectedStudentId = '';
  let presentationTitle = '';
  let starting = false;
  let ending = false;
  let showQR = true;
  let interval = null;

  onMount(async () => {
    if (!$auth.isAuthenticated) { goto('/'); return; }
    await loadAll();
    interval = setInterval(pollActive, 5000);
  });

  onDestroy(() => { if (interval) clearInterval(interval); });

  async function loadAll() {
    loading = true;
    try {
      [classData, qrData] = await Promise.all([
        api.getClass(classId),
        api.getClassQR(classId, window.location.origin)
      ]);
      await pollActive();
    } catch (e) {
      error = e.message;
    } finally {
      loading = false;
    }
  }

  async function pollActive() {
    try {
      const res = await api.getActivePresentationByClassId(classId);
      activePresentation = res.active ? res.presentation : null;
    } catch {}
  }

  async function handleUpload(e) {
    const file = e.target.files[0];
    if (!file) return;
    try {
      const res = await api.uploadStudents(classId, file);
      classData = await api.getClass(classId);
      alert(res.message);
    } catch (e) {
      alert(e.message);
    }
    e.target.value = '';
  }

  async function startPresentation() {
    if (!selectedStudentId) return;
    starting = true;
    error = '';
    try {
      await api.startPresentation({
        presenter_id: parseInt(selectedStudentId),
        title: presentationTitle.trim() || null
      });
      selectedStudentId = '';
      presentationTitle = '';
      await pollActive();
    } catch (e) {
      error = e.message;
    } finally {
      starting = false;
    }
  }

  async function endPresentation() {
    if (!activePresentation) return;
    ending = true;
    try {
      await api.endPresentation(activePresentation.id);
      activePresentation = null;
    } catch (e) {
      error = e.message;
    } finally {
      ending = false;
    }
  }
</script>

<svelte:head><title>{classData?.name || '반 관리'} - 발표 평가 시스템</title></svelte:head>

<div class="max-w-5xl mx-auto px-4 py-8">
  <div class="flex items-center gap-3 mb-6">
    <a href="/teacher/dashboard" class="text-blue-600 hover:text-blue-800 text-sm font-medium">← 대시보드</a>
    <span class="text-gray-300">/</span>
    <span class="text-gray-500 text-sm">{classData?.name || ''}</span>
  </div>

  {#if error}
    <div class="bg-red-50 border border-red-200 text-red-600 px-4 py-3 rounded-xl mb-4 text-sm">{error}</div>
  {/if}

  {#if loading}
    <div class="text-center py-16 text-gray-400">로딩 중...</div>
  {:else if classData}
    <div class="flex flex-col lg:flex-row gap-6">

      <!-- Left column -->
      <div class="lg:w-80 space-y-4 shrink-0">

        <!-- Class info -->
        <div class="card">
          <h1 class="text-xl font-bold text-gray-800">{classData.name}</h1>
          {#if classData.description}
            <p class="text-gray-400 text-sm mt-1">{classData.description}</p>
          {/if}
          <div class="mt-3 flex items-center gap-2 text-sm text-gray-400">
            <span>👥 학생 {classData.students.length}명 등록됨</span>
          </div>
        </div>

        <!-- QR Code -->
        <div class="card">
          <div class="flex justify-between items-center mb-3">
            <h2 class="font-semibold text-gray-700">📱 학생 접속 QR코드</h2>
            <button on:click={() => showQR = !showQR} class="text-xs text-blue-500">
              {showQR ? '숨기기' : '보기'}
            </button>
          </div>
          {#if showQR && qrData}
            <div class="text-center">
              <img src={qrData.qr_data_url} alt="QR Code" class="mx-auto w-48 h-48 rounded-xl border border-gray-100" />
              <div class="mt-2 bg-blue-50 text-blue-700 rounded-lg px-3 py-1.5 text-sm">
                코드: <strong class="font-bold text-base">{qrData.access_code}</strong>
              </div>
              <p class="text-xs text-gray-300 mt-2 break-all">{qrData.access_url}</p>
            </div>
          {:else if !qrData}
            <p class="text-gray-400 text-sm text-center py-2">QR 코드를 불러올 수 없습니다</p>
          {/if}
        </div>

        <!-- Upload students -->
        <div class="card">
          <h2 class="font-semibold text-gray-700 mb-2">📋 학생 명단 업로드</h2>
          <p class="text-xs text-gray-400 mb-3">CSV 형식: <code class="bg-gray-100 px-1 rounded">이름,번호</code> 컨럼 포함</p>
          <label class="cursor-pointer block">
            <span class="block text-center bg-gray-100 hover:bg-gray-200 text-gray-700 py-2.5 px-3 rounded-xl text-sm font-medium transition-colors">
              📁 CSV 파일 선택
            </span>
            <input type="file" accept=".csv" on:change={handleUpload} class="hidden" />
          </label>
        </div>

        <!-- Results link -->
        <a
          href="/teacher/results/{classId}"
          class="block text-center bg-emerald-600 hover:bg-emerald-700 text-white py-3 rounded-xl font-semibold transition-colors"
        >📊 평가 결과 보기</a>
      </div>

      <!-- Right column -->
      <div class="flex-1 space-y-4">

        <!-- Active presentation banner -->
        {#if activePresentation}
          <div class="bg-green-50 border-2 border-green-300 rounded-2xl p-5">
            <div class="flex items-center justify-between">
              <div>
                <div class="flex items-center gap-2 mb-1">
                  <span class="relative flex h-3 w-3">
                    <span class="animate-ping absolute inline-flex h-full w-full rounded-full bg-green-400 opacity-75"></span>
                    <span class="relative inline-flex rounded-full h-3 w-3 bg-green-500"></span>
                  </span>
                  <span class="text-green-700 font-semibold text-sm">발표 진행 중</span>
                </div>
                <div class="text-2xl font-bold text-gray-800">{activePresentation.presenter_name}</div>
                {#if activePresentation.title}
                  <div class="text-green-700 text-sm mt-0.5">"{activePresentation.title}"</div>
                {/if}
              </div>
              <button
                on:click={endPresentation}
                disabled={ending}
                class="bg-red-500 hover:bg-red-600 disabled:bg-red-300 text-white px-5 py-2.5 rounded-xl font-semibold transition-colors"
              >
                {ending ? '종료 중...' : '발표 종료'}
              </button>
            </div>
          </div>
        {:else}
          <div class="bg-gray-50 border border-gray-200 rounded-2xl p-4 text-center text-gray-400 text-sm">
            현재 진행 중인 발표가 없습니다
          </div>
        {/if}

        <!-- Start presentation -->
        <div class="card">
          <h2 class="font-semibold text-gray-700 mb-4">🎯 발표 시작하기</h2>
          {#if classData.students.length === 0}
            <div class="text-center py-6 text-gray-400">
              <p class="text-sm">학생 명단을 먼저 업로드해주세요</p>
            </div>
          {:else}
            <div class="space-y-3">
              <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">발표자 선택 <span class="text-red-400">*</span></label>
                <select bind:value={selectedStudentId} class="input">
                  <option value="">-- 발표자를 선택하세요 --</option>
                  {#each classData.students as s}
                    <option value={s.id}>
                      {s.student_number ? `[${s.student_number}] ` : ''}{s.name}
                    </option>
                  {/each}
                </select>
              </div>
              <div>
                <label class="block text-sm font-medium text-gray-700 mb-1">발표 주제 (선택)</label>
                <input
                  type="text"
                  bind:value={presentationTitle}
                  placeholder="예: 소프트웨어 개발 방법론"
                  class="input"
                />
              </div>
              <button
                on:click={startPresentation}
                disabled={!selectedStudentId || starting}
                class="btn-primary w-full"
              >
                {starting ? '시작 중...' : '🎤 발표 시작'}
              </button>
            </div>
          {/if}
        </div>

        <!-- Student list -->
        <div class="card">
          <h2 class="font-semibold text-gray-700 mb-3">👥 학생 목록 ({classData.students.length}명)</h2>
          {#if classData.students.length === 0}
            <p class="text-gray-400 text-sm text-center py-4">등록된 학생이 없습니다</p>
          {:else}
            <div class="max-h-56 overflow-y-auto">
              <div class="grid grid-cols-2 sm:grid-cols-3 gap-2">
                {#each classData.students as s}
                  <div class="bg-gray-50 rounded-lg px-3 py-2 text-sm flex items-center gap-1.5 {
                    activePresentation?.presenter_id === s.id ? 'bg-green-50 border border-green-200' : ''
                  }">
                    {#if s.student_number}
                      <span class="text-gray-300 text-xs w-5 shrink-0">{s.student_number}</span>
                    {/if}
                    <span class="font-medium text-gray-700 truncate">{s.name}</span>
                    {#if activePresentation?.presenter_id === s.id}
                      <span class="text-green-600 text-xs ml-auto">🎤</span>
                    {/if}
                  </div>
                {/each}
              </div>
            </div>
          {/if}
        </div>

      </div>
    </div>
  {/if}
</div>
