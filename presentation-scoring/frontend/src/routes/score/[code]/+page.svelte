<script>
  import { onMount, onDestroy } from 'svelte';
  import { page } from '$app/stores';
  import { api } from '$lib/api';

  const code = $page.params.code;

  let classInfo = null;
  let activePresentation = null;
  let loading = true;
  let error = '';
  let selectedStudentId = '';
  let selectedScore = 0;
  let hoverScore = 0;
  let comment = '';
  let submitting = false;
  let submitted = false;
  let submitError = '';
  let interval = null;

  onMount(async () => {
    await loadClass();
    await poll();
    interval = setInterval(poll, 5000);
  });

  onDestroy(() => { if (interval) clearInterval(interval); });

  async function loadClass() {
    loading = true;
    try {
      classInfo = await api.getPublicClass(code);
    } catch {
      error = 'QR코드를 다시 확인해주세요.\n(반을 찾을 수 없습니다)';
    } finally {
      loading = false;
    }
  }

  async function poll() {
    try {
      const res = await api.getActivePresentation(code);
      const prevId = activePresentation?.id;
      if (res.active) {
        if (res.presentation.id !== prevId) {
          // New presentation started
          submitted = false;
          selectedScore = 0;
          comment = '';
          submitError = '';
        }
        activePresentation = res.presentation;
      } else {
        activePresentation = null;
      }
    } catch {}
  }

  async function submit() {
    if (!selectedStudentId || selectedScore === 0 || !activePresentation) return;
    submitting = true;
    submitError = '';
    try {
      await api.submitScore({
        score: selectedScore,
        comment: comment.trim() || null,
        presentation_id: activePresentation.id,
        evaluator_id: parseInt(selectedStudentId)
      });
      submitted = true;
    } catch (e) {
      submitError = e.message;
    } finally {
      submitting = false;
    }
  }

  $: filteredStudents = classInfo?.students?.filter(
    (s) => s.id !== activePresentation?.presenter_id
  ) ?? [];

  const scoreLabels = {
    1: '매우 부족',
    2: '부족',
    3: '문제있음',
    4: '보통 이하',
    5: '보통',
    6: '보통 이상',
    7: '양호',
    8: '좋음',
    9: '매우 좋음',
    10: '탁월'
  };
</script>

<svelte:head>
  <title>발표 평가{classInfo ? ' - ' + classInfo.class_name : ''}</title>
</svelte:head>

<div class="min-h-screen bg-gradient-to-br from-blue-50 via-white to-indigo-50">
  <div class="max-w-lg mx-auto px-4 py-8 pb-16">

    <!-- Header -->
    <div class="text-center mb-6">
      <div class="text-5xl mb-2">🎤</div>
      <h1 class="text-2xl font-bold text-gray-800">발표 평가</h1>
      {#if classInfo}
        <p class="text-gray-500 text-sm mt-1">{classInfo.class_name}</p>
      {/if}
    </div>

    {#if loading}
      <div class="card text-center py-12">
        <div class="text-4xl mb-3 animate-pulse">⏳</div>
        <p class="text-gray-400">로딩 중...</p>
      </div>

    {:else if error}
      <div class="card text-center py-10">
        <div class="text-4xl mb-3">❌</div>
        <p class="text-gray-600 whitespace-pre-line">{error}</p>
      </div>

    {:else if !activePresentation}
      <div class="card text-center py-12">
        <div class="text-6xl mb-4 animate-pulse">⏳</div>
        <h2 class="text-xl font-bold text-gray-700 mb-2">발표 대기 중</h2>
        <p class="text-gray-400 text-sm">교사님이 발표를 시작하면<br/>자동으로 평가 화면이 나타납니다</p>
        <p class="text-gray-300 text-xs mt-6">5초마다 자동 확인 중...</p>
      </div>

    {:else if submitted}
      <div class="card text-center py-10">
        <div class="text-6xl mb-4">✅</div>
        <h2 class="text-2xl font-bold text-emerald-700 mb-2">평가 완료!</h2>
        <p class="text-gray-500 mb-4">
          <strong class="text-gray-800 text-lg">{activePresentation.presenter_name}</strong>
          학생의 발표를
          <strong class="text-blue-600">{selectedScore}점</strong>으로 평가했습니다
        </p>
        {#if comment}
          <div class="bg-gray-50 rounded-xl p-4 text-gray-600 text-sm italic mb-4">
            "{comment}"
          </div>
        {/if}
        <p class="text-gray-300 text-xs mt-4">다음 발표가 시작되면 다시 평가할 수 있습니다</p>
      </div>

    {:else}
      <div class="space-y-4">

        <!-- Presenter info -->
        <div class="card bg-gradient-to-r from-blue-600 to-blue-700 text-white border-0">
          <p class="text-blue-200 text-xs font-semibold uppercase tracking-wide mb-1">현재 발표자</p>
          <h2 class="text-3xl font-bold">{activePresentation.presenter_name}</h2>
          {#if activePresentation.title}
            <p class="text-blue-200 text-sm mt-1.5">📌 {activePresentation.title}</p>
          {/if}
        </div>

        <!-- Who am I -->
        <div class="card">
          <h3 class="font-semibold text-gray-700 mb-2">👤 내 이름 선택</h3>
          <select bind:value={selectedStudentId} class="input text-base">
            <option value="">-- 내 이름을 선택하세요 --</option>
            {#each filteredStudents as s}
              <option value={s.id}>
                {s.student_number ? `${s.student_number}번 ` : ''}{s.name}
              </option>
            {/each}
          </select>
        </div>

        <!-- Score -->
        <div class="card">
          <h3 class="font-semibold text-gray-700 mb-1">⭐ 점수 <span class="text-gray-400 font-normal text-sm">(1~10점)</span></h3>
          {#if selectedScore > 0}
            <p class="text-sm text-blue-600 mb-3 font-medium">{scoreLabels[selectedScore]}</p>
          {:else}
            <p class="text-sm text-gray-300 mb-3">점수를 선택해주세요</p>
          {/if}
          <div class="grid grid-cols-5 gap-2">
            {#each [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] as n}
              <button
                on:click={() => (selectedScore = n)}
                on:mouseenter={() => (hoverScore = n)}
                on:mouseleave={() => (hoverScore = 0)}
                class="aspect-square rounded-xl text-lg font-bold transition-all border-2 {
                  selectedScore === n
                    ? 'bg-blue-600 text-white border-blue-600 scale-110 shadow-md'
                    : hoverScore === n
                    ? 'bg-blue-50 border-blue-300 text-blue-700'
                    : 'bg-gray-50 border-gray-100 text-gray-500 hover:border-blue-200'
                }"
              >{n}</button>
            {/each}
          </div>
        </div>

        <!-- Comment -->
        <div class="card">
          <h3 class="font-semibold text-gray-700 mb-2">💬 코멘트 <span class="text-gray-400 font-normal text-sm">(선택)</span></h3>
          <textarea
            bind:value={comment}
            placeholder="발표에 대한 피드백을 적어주세요..."
            rows="3"
            class="input resize-none"
          ></textarea>
        </div>

        {#if submitError}
          <div class="bg-red-50 border border-red-200 text-red-600 px-4 py-3 rounded-xl text-sm">
            {submitError}
          </div>
        {/if}

        <button
          on:click={submit}
          disabled={!selectedStudentId || selectedScore === 0 || submitting}
          class="btn-primary w-full text-lg py-4 shadow-lg"
        >
          {submitting ? '제출 중...' : '🚀 평가 제출하기'}
        </button>
      </div>
    {/if}
  </div>
</div>
