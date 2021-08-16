package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"strings"

	"github.com/kjk/u"
)

// https://goobar.io/2019/12/07/manually-trigger-a-github-actions-workflow/
// send a webhook POST request to trigger a build
func triggerBuildWebHook(typ string) {
	ghtoken := os.Getenv("GITHUB_TOKEN")
	panicIf(ghtoken == "", "need GITHUB_TOKEN env variable")
	data := fmt.Sprintf(`{"event_type": "%s"}`, typ)
	uri := "https://api.github.com/repos/sumatrapdfreader/sumatrapdf/dispatches"
	req, err := http.NewRequest(http.MethodPost, uri, strings.NewReader(data))
	u.Must(err)
	req.Header.Set("Accept", "application/vnd.github.everest-preview+json")
	val := fmt.Sprintf("token %s", ghtoken)
	req.Header.Set("Authorization", val)
	rsp, err := http.DefaultClient.Do(req)
	u.Must(err)
	panicIf(rsp.StatusCode >= 400)
}

const (
	githubEventTypeCodeQL = "codeql"
	githubEventPush       = "push"
)

//  "action": "build-pre-rel"
type gitHubEventJSON struct {
	Action string `json:"action"`
}

func getGitHubEventType() string {
	v := os.Getenv("GITHUB_EVENT_NAME")
	isWebhookDispatch := v == "repository_dispatch"
	if !isWebhookDispatch {
		return githubEventPush
	}
	path := os.Getenv("GITHUB_EVENT_PATH")
	d, err := ioutil.ReadFile(path)
	must(err)
	var js gitHubEventJSON
	err = json.Unmarshal(d, &js)
	must(err)
	// validate this is an action we understand
	switch js.Action {
	case githubEventTypeCodeQL:
		return js.Action
	}
	panicIf(true, "invalid js.Action of '%s'", js.Action)
	return ""
}

// https://help.github.com/en/actions/configuring-and-managing-workflows/using-environment-variables#default-environment-variables
func dumpWebHookEventPayload() {
	v := os.Getenv("GITHUB_EVENT_PATH")
	d, err := ioutil.ReadFile(v)
	if err != nil {
		fmt.Printf("dumpWebHookEventPayload: GITHUB_EVENT_PATH='%s' and is not a file\n", v)
	}
	fmt.Printf("dumpWebHookEventPayload: GITHUB_EVENT_PATH='%s'. Content:\n%s\n", v, string(d))
}
