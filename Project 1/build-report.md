# Report
Date: Sun 08 Nov 2020 07:41:52 PM WET  
Repo: git@git.rnl.tecnico.ulisboa.pt:RC-20-21/ist193695-proj1.git  
Commit: dedc1d59ce03b02b80a4a77a51ee4359572884d6  

## Build
* Found `Makefile`.
* Build succeeded.
* Found `file-sender`.
* Found `file-receiver`.

## Tests
| Test | Result |
| ---- |:------:|
| Sending small text file | OK |
| Sending binary file | OK |
| Sending 500 byte file | OK |
| Sending 1000 byte file | OK |
| Stop & Wait. No Loss | OK |
| Stop & Wait. Loss | OK |
| Go Back N. No Loss | OK |
| Go Back N. Loss | OK |
| Selective Repeat. No Loss | OK |
| Selective Repeat. Loss | **FAIL** |
| Message format | **FAIL** |
| Triple loss. | OK |

## Submission
* Found `project1-submission` tag. Project is ready for grading.
* `project1-submission` tag matches `master` branch. Submission is up to date.
